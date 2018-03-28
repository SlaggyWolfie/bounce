/*
* Copyright (c) 2016-2016 Irlan Robson http://www.irlan.net
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

#ifndef B3_SPRING_CLOTH_H
#define B3_SPRING_CLOTH_H

#include <bounce/common/math/mat33.h>

#define B3_CLOTH_SHAPE_CAPACITY 32

class b3StackAllocator;
class b3Draw;

class b3Shape;

struct b3Mesh;

struct b3SpringClothDef
{
	b3SpringClothDef()
	{
		allocator = nullptr;
		mesh = nullptr;
		density = 0.0f;
		ks = 0.0f;
		kb = 0.0f;
		kd = 0.0f;
		r = 0.05f;
		gravity.SetZero();
	}

	// Stack allocator
	b3StackAllocator* allocator;

	// Cloth mesh	
	b3Mesh* mesh;

	// Cloth density in kg/m^2
	float32 density;

	// Streching stiffness
	float32 ks;

	// Bending stiffness
	float32 kb;

	// Damping stiffness
	float32 kd;
	
	// Mass radius
	float32 r;
	
	// Force due to gravity
	b3Vec3 gravity;
};

enum b3SpringType
{
	e_strechSpring,
	e_bendSpring
};

struct b3Spring
{
	// Spring type
	b3SpringType type;

	// Mass 1
	u32 i1;

	// Mass 2
	u32 i2;

	// Rest length
	float32 L0;

	// Structural stiffness
	float32 ks;
	
	// Damping stiffness
	float32 kd;
};

// Static masses have zero mass and velocity, and therefore they can't move.
// Dynamic masses have non-zero mass and can move due to internal and external forces.
enum b3MassType
{
	e_staticMass,
	e_dynamicMass
};

// 
struct b3MassContact
{
	b3Vec3 n, t1, t2;
	float32 Fn, Ft1, Ft2;
	u32 j;
	bool lockOnSurface, slideOnSurface;
};

// Time step statistics
struct b3SpringClothStep
{
	u32 iterations;
};

// This class implements a cloth. It treats cloth as a collection 
// of masses connected by springs. 
// Large time steps can be taken.
// If accuracy and stability are required, not performance, 
// you can use this class instead of using b3Cloth.
class b3SpringCloth
{
public:
	b3SpringCloth();
	~b3SpringCloth();

	//
	void Initialize(const b3SpringClothDef& def);

	//
	b3Mesh* GetMesh() const;

	//
	void SetGravity(const b3Vec3& gravity);

	//
	const b3Vec3& GetGravity() const;

	//
	void SetType(u32 i, b3MassType type);

	//
	b3MassType GetType(u32 i) const;

	// Note, the position will be changed only after performing a time step.
	void SetPosition(u32 i, const b3Vec3& translation);

	//
	const b3Vec3& GetPosition(u32 i) const;

	//
	void ApplyForce(u32 i, const b3Vec3& force);

	//
	float32 GetKineticEnergy() const;
	
	//
	void AddShape(b3Shape* shape);

	// 
	u32 GetShapeCount() const;

	// 
	b3Shape** GetShapes();

	// 
	const b3SpringClothStep& GetStep() const;

	//
	void Step(float32 dt);

	//
	void Apply() const;

	//
	void Draw(b3Draw* draw) const;
protected:
	friend class b3SpringSolver;
	
	// Update contacts. 
	// This is where some contacts might be initiated or terminated.
	void UpdateContacts();
	
	b3StackAllocator* m_allocator;

	b3Mesh* m_mesh;
	float32 m_r;

	b3Vec3 m_gravity;

	b3Vec3* m_x;
	b3Vec3* m_v;
	b3Vec3* m_f;
	float32* m_m;
	float32* m_inv_m;
	b3Vec3* m_y;
	b3MassType* m_types;
	u32 m_massCount;

	b3MassContact* m_contacts;
	
	b3Spring* m_springs;
	u32 m_springCount;

	b3Shape* m_shapes[B3_CLOTH_SHAPE_CAPACITY];
	u32 m_shapeCount;

	b3SpringClothStep m_step;
};

inline b3Mesh* b3SpringCloth::GetMesh() const
{
	return m_mesh;
}

inline const b3Vec3& b3SpringCloth::GetGravity() const
{
	return m_gravity;
}

inline void b3SpringCloth::SetGravity(const b3Vec3& gravity)
{
	m_gravity = gravity;
}

inline b3MassType b3SpringCloth::GetType(u32 i) const
{
	B3_ASSERT(i < m_massCount);
	return m_types[i];
}

inline void b3SpringCloth::SetType(u32 i, b3MassType type)
{
	B3_ASSERT(i < m_massCount);
	if (m_types[i] == type)
	{
		return;
	}

	m_types[i] = type;
	
	m_f[i].SetZero();
	
	if (type == e_staticMass)
	{
		m_v[i].SetZero();
		m_y[i].SetZero();
		
		m_contacts[i].lockOnSurface = false;
	}
}

inline void b3SpringCloth::SetPosition(u32 i, const b3Vec3& position)
{
	B3_ASSERT(i < m_massCount);
	m_y[i] += position - m_x[i];
}

inline const b3Vec3& b3SpringCloth::GetPosition(u32 i) const
{
	B3_ASSERT(i < m_massCount);
	return m_x[i];
}

inline void b3SpringCloth::ApplyForce(u32 i, const b3Vec3& force)
{
	B3_ASSERT(i < m_massCount);
	
	if (m_types[i] != e_dynamicMass)
	{
		return;
	}

	m_f[i] += force;
}

inline float32 b3SpringCloth::GetKineticEnergy() const
{
	float32 E = 0.0f;
	for (u32 i = 0; i < m_massCount; ++i)
	{
		b3Vec3 P = m_m[i] * m_v[i];
		E += b3Dot(P, m_v[i]);
	}
	return E;
}

inline u32 b3SpringCloth::GetShapeCount() const
{
	return m_shapeCount;
}

inline b3Shape** b3SpringCloth::GetShapes() 
{
	return m_shapes;
}

inline const b3SpringClothStep& b3SpringCloth::GetStep() const
{
	return m_step;
}

#endif