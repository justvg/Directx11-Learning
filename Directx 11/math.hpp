#pragma once

#include <stdint.h>
#include <math.h>
#include <xmmintrin.h>

#define PI 3.14159265358979323846f
#define INT_MIN (-2147483647 - 1)
#define INT_MAX 2147483647
#define FLT_MAX 3.402823466e+38F
#define DEG2RAD(Deg) ((Deg)*PI/180.0f)
#define RAD2DEG(Rad) ((Rad)*180.0f/PI)

typedef float real32;
typedef double real64;

#define internal static
#define global_variable static

//
// NOTE(georgy): v2
//

union v2
{
	struct
	{
		real32 x, y;
	};
	struct
	{
		real32 u, v;
	};
	real32 E[2];

	void Normalize();
};

inline v2
V2(real32 X, real32 Y)
{
	v2 Result;

	Result.x = X;
	Result.y = Y;

	return(Result);
}

inline v2
operator*(real32 A, v2 B)
{
	v2 Result;

	Result.x = A * B.x;
	Result.y = A * B.y;

	return(Result);
}

inline v2
operator*(v2 B, real32 A)
{
	v2 Result = A * B;

	return(Result);
}

inline v2 &
operator*=(v2 &A, real32 B)
{
	A = B * A;

	return(A);
}

inline v2
operator-(v2 A)
{
	v2 Result;

	Result.x = -A.x;
	Result.y = -A.y;

	return(Result);
}

inline v2
operator+(v2 A, v2 B)
{
	v2 Result;

	Result.x = A.x + B.x;
	Result.y = A.y + B.y;

	return(Result);
}

inline v2 &
operator+=(v2 &A, v2 B)
{
	A = A + B;

	return(A);
}

inline v2
operator-(v2 A, v2 B)
{
	v2 Result;

	Result.x = A.x - B.x;
	Result.y = A.y - B.y;

	return(Result);
}

inline v2 &
operator-=(v2 &A, v2 B)
{
	A = A - B;

	return(A);
}

inline v2
Hadamard(v2 A, v2 B)
{
	v2 Result;

	Result.x = A.x * B.x;
	Result.y = A.y * B.y;

	return(Result);
}

inline real32
Dot(v2 A, v2 B)
{
	real32 Result = A.x*B.x + A.y*B.y;

	return(Result);
}

inline real32
LengthSq(v2 A)
{
	real32 Result = Dot(A, A);

	return(Result);
}

inline real32
Length(v2 A)
{
	real32 Result = sqrtf(LengthSq(A));

	return(Result);
}

inline v2
Normalize(v2 A)
{
	v2 Result;

	real32 InvLen = 1.0f / Length(A);
	Result = InvLen * A;

	return(Result);
}

inline void
v2::Normalize()
{
	real32 InvLen = 1.0f / Length(*this);

	*this *= InvLen;
}

//
// NOTE(georgy): v3
//

union v3
{
	struct
	{
		real32 x, y, z;
	};
	struct
	{
		real32 u, v, w;
	};
	struct
	{
		real32 r, g, b;
	};
	struct
	{
		v2 xy;
		real32 Ignored_0;
	};
	struct
	{
		real32 Ignored_1;
		v2 yz;
	};
	struct
	{
		v2 uv;
		real32 Ignored_2;
	};
	struct
	{
		real32 Ignored_3;
		v2 vw;
	};
	real32 E[3];

	void Normalize();
};

inline v3
V3(real32 X, real32 Y, real32 Z)
{
	v3 Result;

	Result.x = X;
	Result.y = Y;
	Result.z = Z;

	return(Result);
}

inline v3
operator*(real32 A, v3 B)
{
	v3 Result;

	Result.x = A * B.x;
	Result.y = A * B.y;
	Result.z = A * B.z;

	return(Result);
}

inline v3
operator*(v3 B, real32 A)
{
	v3 Result = A * B;

	return(Result);
}

inline v3 &
operator*=(v3 &A, real32 B)
{
	A = B * A;

	return(A);
}

inline v3
operator-(v3 A)
{
	v3 Result;

	Result.x = -A.x;
	Result.y = -A.y;
	Result.z = -A.z;

	return(Result);
}

inline v3
operator+(v3 A, v3 B)
{
	v3 Result;

	Result.x = A.x + B.x;
	Result.y = A.y + B.y;
	Result.z = A.z + B.z;

	return(Result);
}

inline v3 &
operator+=(v3 &A, v3 B)
{
	A = A + B;

	return(A);
}

inline v3
operator-(v3 A, v3 B)
{
	v3 Result;

	Result.x = A.x - B.x;
	Result.y = A.y - B.y;
	Result.z = A.z - B.z;

	return(Result);
}

inline v3 &
operator-=(v3 &A, v3 B)
{
	A = A - B;

	return(A);
}

inline v3
Hadamard(v3 A, v3 B)
{
	v3 Result;

	Result.x = A.x * B.x;
	Result.y = A.y * B.y;
	Result.z = A.z * B.z;

	return(Result);
}

inline real32
Dot(v3 A, v3 B)
{
	real32 Result = A.x*B.x + A.y*B.y + A.z*B.z;

	return(Result);
}

inline real32
LengthSq(v3 A)
{
	real32 Result = Dot(A, A);

	return(Result);
}

inline real32
Length(v3 A)
{
	real32 Result = sqrtf(LengthSq(A));

	return(Result);
}

inline v3
Cross(v3 A, v3 B)
{
	v3 Result;

	Result.x = A.y*B.z - A.z*B.y;
	Result.y = A.z*B.x - A.x*B.z;
	Result.z = A.x*B.y - A.y*B.x;

	return(Result);
}

inline v3
Normalize(v3 A)
{
	v3 Result;

	real32 InvLen = 1.0f / Length(A);
	Result = InvLen * A;

	return(Result);
}

inline void
v3::Normalize()
{
	real32 InvLen = 1.0f / Length(*this);

	*this *= InvLen;
}

//
// NOTE(georgy): v4
//

union v4
{
	struct
	{
		real32 x, y, z, w;
	};
	struct
	{
		real32 r, g, b, a;
	};
	struct
	{
		v3 xyz;
		real32 Ignored_0;
	};
	struct
	{
		v3 rgb;
		real32 Ignored_1;
	};
	struct
	{
		v2 xy;
		real32 Ignored_2;
		real32 Ignored_3;
	};
	struct
	{
		real32 Ignored_4;
		v2 yz;
		real32 Ignored_5;
	};
	struct
	{
		real32 Ignored_6;
		real32 Ignored_7;
		v2 zw;
	};
	real32 E[4];

	void Normalize();
};

inline v4
V4(real32 X, real32 Y, real32 Z, real32 W)
{
	v4 Result;

	Result.x = X;
	Result.y = Y;
	Result.z = Z;
	Result.w = W;

	return(Result);
}

inline v4
V4(v3 A, real32 W)
{
	v4 Result;

	Result.x = A.x;
	Result.y = A.y;
	Result.z = A.z;
	Result.w = A.w;

	return(Result);
}

inline v4
operator*(real32 A, v4 B)
{
	v4 Result;

	Result.x = A * B.x;
	Result.y = A * B.y;
	Result.z = A * B.z;
	Result.w = A * B.w;

	return(Result);
}

inline v4
operator*(v4 B, real32 A)
{
	v4 Result = A * B;

	return(Result);
}

inline v4 &
operator*=(v4 &A, real32 B)
{
	A = B * A;

	return(A);
}

inline v4
operator-(v4 A)
{
	v4 Result;

	Result.x = -A.x;
	Result.y = -A.y;
	Result.z = -A.z;
	Result.w = -A.w;

	return(Result);
}

inline v4
operator+(v4 A, v4 B)
{
	v4 Result;

	Result.x = A.x + B.x;
	Result.y = A.y + B.y;
	Result.z = A.z + B.z;
	Result.w = A.w + B.w;

	return(Result);
}

inline v4 &
operator+=(v4 &A, v4 B)
{
	A = A + B;

	return(A);
}

inline v4
operator-(v4 A, v4 B)
{
	v4 Result;

	Result.x = A.x - B.x;
	Result.y = A.y - B.y;
	Result.z = A.z - B.z;
	Result.w = A.w - B.w;

	return(Result);
}

inline v4 &
operator-=(v4 &A, v4 B)
{
	A = A - B;

	return(A);
}

inline v4
Hadamard(v4 A, v4 B)
{
	v4 Result;

	Result.x = A.x * B.x;
	Result.y = A.y * B.y;
	Result.z = A.z * B.z;
	Result.w = A.w * B.w;

	return(Result);
}

inline real32
Dot(v4 A, v4 B)
{
	real32 Result = A.x*B.x + A.y*B.y + A.z*B.z + A.w*B.w;

	return(Result);
}

inline real32
LengthSq(v4 A)
{
	real32 Result = Dot(A, A);

	return(Result);
}

inline real32
Length(v4 A)
{
	real32 Result = sqrtf(LengthSq(A));

	return(Result);
}

inline v4
Normalize(v4 A)
{
	v4 Result;

	real32 InvLen = 1.0f / Length(A);
	Result = InvLen * A;

	return(Result);
}

inline void
v4::Normalize()
{
	real32 InvLen = 1.0f / Length(*this);

	*this *= InvLen;
}

//
// NOTE(georgy): mat4
//

struct mat4
{
	union
	{
		real32 Elements[16];
		struct
		{
			real32 a11, a21, a31, a41;
			real32 a12, a22, a32, a42;
			real32 a13, a23, a33, a43;
			real32 a14, a24, a34, a44;
		};
	};
};

internal mat4 
operator*(mat4 A, mat4 B)
{
	mat4 Result;

	for (uint32_t I = 0; I < 4; I++)
	{
		for (uint32_t J = 0; J < 4; J++)
		{
			real32 Sum = 0.0f;
			for (uint32_t E = 0; E < 4; E++)
			{
				Sum += A.Elements[I + E*4] * B.Elements[J*4 + E];
			}
			Result.Elements[I + J*4] = Sum;
		}
	}

	return(Result);
}

inline mat4 
Identity(float Diagonal = 1.0f)
{
	mat4 Result = {};

	Result.a11 = Diagonal;
	Result.a22 = Diagonal;
	Result.a33 = Diagonal;
	Result.a44 = Diagonal;

	return(Result);
}

inline mat4
Translate(v3 Translation)
{
	mat4 Result = Identity(1.0f);

	Result.a41 = Translation.x;
	Result.a42 = Translation.y;
	Result.a43 = Translation.z;

	return(Result);
}

inline mat4
Scale(real32 Scale)
{
	mat4 Result = {};

	Result.a11 = Scale;
	Result.a22 = Scale;
	Result.a33 = Scale;
	Result.a44 = 1.0f;

	return(Result);
}

inline mat4
Scale(v3 Scale)
{
	mat4 Result = {};

	Result.a11 = Scale.x;
	Result.a22 = Scale.y;
	Result.a33 = Scale.z;
	Result.a44 = 1.0f;

	return(Result);
}

internal mat4
Rotate(real32 Angle, v3 Axis)
{
	mat4 Result;

	real32 Rad = DEG2RAD(Angle);
	real32 Cos = cosf(Rad);
	real32 Sin = sinf(Rad);
	Axis.Normalize();

	float OneMinusCosine = 1.0f - Cos;

	Result.a11 = Axis.x*Axis.x*OneMinusCosine + Cos;
	Result.a21 = Axis.x*Axis.y*OneMinusCosine + Axis.z*Sin;
	Result.a31 = Axis.x*Axis.z*OneMinusCosine - Axis.y*Sin;
	Result.a41 = 0.0f;

	Result.a12 = Axis.x*Axis.y*OneMinusCosine - Axis.z*Sin;
	Result.a22 = Axis.y*Axis.y*OneMinusCosine + Cos;
	Result.a32 = Axis.y*Axis.z*OneMinusCosine + Axis.x*Sin;
	Result.a42 = 0.0f;

	Result.a13 = Axis.x*Axis.z*OneMinusCosine + Axis.y*Sin;
	Result.a23 = Axis.y*Axis.z*OneMinusCosine - Axis.x*Sin;
	Result.a33 = Axis.z*Axis.z*OneMinusCosine + Cos;
	Result.a43 = 0.0f;

	Result.a14 = 0.0f;
	Result.a24 = 0.0f;
	Result.a34 = 0.0f;
	Result.a44 = 1.0f;

	return(Result);
}

internal mat4
LookAt(v3 From, v3 Target, v3 UpAxis = V3(0.0f, 1.0f, 0.0f))
{
	v3 Forward = Normalize(Target - From);
	v3 Right = Normalize(Cross(UpAxis, Forward));
	v3 Up = Normalize(Cross(Forward, Right));

	mat4 Result;

	Result.a11 = Right.x;
	Result.a21 = Right.y;
	Result.a31 = Right.z;

	Result.a12 = Up.x;
	Result.a22 = Up.y;
	Result.a32 = Up.z;

	Result.a13 = Forward.x;
	Result.a23 = Forward.y;
	Result.a33 = Forward.z;

	Result.a41 = -Dot(From, Right);
	Result.a42 = -Dot(From, Up);
	Result.a43 = -Dot(From, Forward);
	
	Result.a14 = 0.0f;
	Result.a24 = 0.0f;
	Result.a34 = 0.0f;
	Result.a44 = 1.0f;

	return(Result);
}

internal mat4 __vectorcall
Perspective(real32 FoV, real32 AspectRatio, real32 Near, real32 Far)
{
	real32 ZoomY = 1.0f / tanf(DEG2RAD(FoV)*0.5f);
	// TODO(georgy): ZoomX isn't exactly right!
	real32 ZoomX = ZoomY * (1.0f / AspectRatio);

	mat4 Result;

	real32 FRange = Far / (Far - Near);

	Result.a11 = ZoomX;
	Result.a21 = 0.0f;
	Result.a31 = 0.0f;
	Result.a41 = 0.0f;

	Result.a12 = 0.0f;
	Result.a22 = ZoomY;
	Result.a32 = 0.0f;
	Result.a42 = 0.0f;

	Result.a13 = 0.0f;
	Result.a23 = 0.0f;
	Result.a33 = FRange;
	Result.a43 = -FRange*Near;
	
	Result.a14 = 0.0f;
	Result.a24 = 0.0f;
	Result.a34 = 1.0f;
	Result.a44 = 0.0f;

	return(Result);
}