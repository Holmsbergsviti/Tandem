#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

/** Tiny mesh builder for procedural geometry (car body, wheels, track). Vertex-coloured. */
struct FProcGeo
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;

	int32 AddVert(const FVector& P, const FLinearColor& C, const FVector2D& UV = FVector2D::ZeroVector)
	{
		Verts.Add(P); Colors.Add(C); UVs.Add(UV);
		return Verts.Num() - 1;
	}
	void Tri(int32 A, int32 B, int32 C) { Tris.Add(A); Tris.Add(B); Tris.Add(C); }
	void Quad(int32 A, int32 B, int32 C, int32 D) { Tri(A, B, C); Tri(A, C, D); }

	/** Flat-shaded box. */
	void AddBox(const FVector& Center, const FVector& Half, const FLinearColor& C, const FQuat& Rot = FQuat::Identity)
	{
		static const int32 Faces[6][4] = { {0,1,3,2}, {4,6,7,5}, {0,4,5,1}, {2,3,7,6}, {0,2,6,4}, {1,5,7,3} };
		FVector Corner[8];
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector L((i & 4) ? Half.X : -Half.X, (i & 2) ? Half.Y : -Half.Y, (i & 1) ? Half.Z : -Half.Z);
			Corner[i] = Center + Rot.RotateVector(L);
		}
		for (int32 f = 0; f < 6; ++f)
		{
			const int32 I0 = AddVert(Corner[Faces[f][0]], C);
			const int32 I1 = AddVert(Corner[Faces[f][1]], C);
			const int32 I2 = AddVert(Corner[Faces[f][2]], C);
			const int32 I3 = AddVert(Corner[Faces[f][3]], C);
			Quad(I0, I1, I2, I3);
		}
	}

	/** Smooth tube through closed rings (all rings same point count), optional end caps. */
	void Loft(const TArray<TArray<FVector>>& Rings, TFunctionRef<FLinearColor(int32 RingIdx, int32 PointIdx)> ColorFn, bool bCaps)
	{
		if (Rings.Num() < 2) return;
		const int32 M = Rings[0].Num();
		const int32 Base = Verts.Num();
		for (int32 r = 0; r < Rings.Num(); ++r)
			for (int32 k = 0; k < M; ++k)
				AddVert(Rings[r][k], ColorFn(r, k), FVector2D((float)k / M, (float)r / Rings.Num()));
		for (int32 r = 0; r + 1 < Rings.Num(); ++r)
			for (int32 k = 0; k < M; ++k)
			{
				const int32 K1 = (k + 1) % M;
				Quad(Base + r * M + k, Base + (r + 1) * M + k, Base + (r + 1) * M + K1, Base + r * M + K1);
			}
		if (!bCaps) return;
		for (int32 End = 0; End < 2; ++End)
		{
			const int32 r = End == 0 ? 0 : Rings.Num() - 1;
			FVector Centroid = FVector::ZeroVector;
			for (const FVector& P : Rings[r]) Centroid += P;
			Centroid /= M;
			const int32 CI = AddVert(Centroid, ColorFn(r, 0));
			const int32 First = Verts.Num();
			for (int32 k = 0; k < M; ++k) AddVert(Rings[r][k], ColorFn(r, k));
			for (int32 k = 0; k < M; ++k) Tri(CI, First + k, First + (k + 1) % M);
		}
	}

	/** Surface of revolution around the Y axis. Profile X = radius, Y = position on the axis. */
	void Revolve(const TArray<FVector2D>& Profile, int32 Segs, const FLinearColor& C)
	{
		const int32 P = Profile.Num();
		const int32 Base = Verts.Num();
		for (int32 s = 0; s <= Segs; ++s)
		{
			const float A = 2.f * PI * s / Segs;
			for (int32 k = 0; k < P; ++k)
			{
				AddVert(FVector(Profile[k].X * FMath::Cos(A), Profile[k].Y, Profile[k].X * FMath::Sin(A)), C);
			}
		}
		for (int32 s = 0; s < Segs; ++s)
			for (int32 k = 0; k + 1 < P; ++k)
				Quad(Base + s * P + k, Base + (s + 1) * P + k, Base + (s + 1) * P + k + 1, Base + s * P + k + 1);
	}

	/** Ribbon between two polylines of equal length (A[i] -> B[i]). */
	void Ribbon(const TArray<FVector>& A, const TArray<FVector>& B, TFunctionRef<FLinearColor(int32)> ColorFn, float VScale = 0.001f)
	{
		const int32 Base = Verts.Num();
		for (int32 i = 0; i < A.Num(); ++i)
		{
			AddVert(A[i], ColorFn(i), FVector2D(0.f, i * VScale));
			AddVert(B[i], ColorFn(i), FVector2D(1.f, i * VScale));
		}
		for (int32 i = 0; i + 1 < A.Num(); ++i)
			Quad(Base + i * 2, Base + (i + 1) * 2, Base + (i + 1) * 2 + 1, Base + i * 2 + 1);
	}

	/** Optional baked normals for the first Normals.Num() vertices (see Append / BakeNormals). */
	TArray<FVector> Normals;

	/** Per-vertex normals from the triangles. bWeld also smooths across vertices that share a position. */
	TArray<FVector> ComputeNormals(bool bWeld) const
	{
		TArray<FVector> N;
		N.Init(FVector::ZeroVector, Verts.Num());
		for (int32 t = 0; t + 2 < Tris.Num(); t += 3)
		{
			// UE treats counter-clockwise (on screen) as the front face, so the visible-side normal is
			// (C - A) x (B - A). The old (B - A) x (C - A) pointed every normal away from the camera,
			// which left the car, road and kerbs unlit (black) from outside.
			const FVector F = FVector::CrossProduct(Verts[Tris[t + 2]] - Verts[Tris[t]], Verts[Tris[t + 1]] - Verts[Tris[t]]);
			N[Tris[t]] += F; N[Tris[t + 1]] += F; N[Tris[t + 2]] += F;
		}
		if (bWeld)
		{
			auto Key = [](const FVector& P) { return FIntVector(FMath::RoundToInt(P.X * 10.f), FMath::RoundToInt(P.Y * 10.f), FMath::RoundToInt(P.Z * 10.f)); };
			TMap<FIntVector, FVector> Acc;
			for (int32 i = 0; i < Verts.Num(); ++i) Acc.FindOrAdd(Key(Verts[i])) += N[i];
			for (int32 i = 0; i < Verts.Num(); ++i) N[i] = Acc.FindChecked(Key(Verts[i]));
		}
		for (FVector& V : N) V = V.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
		return N;
	}

	void BakeNormals(bool bWeld) { Normals = ComputeNormals(bWeld); }

	/** Adds another mesh, keeping the normals each part already has. */
	void Append(const FProcGeo& O)
	{
		if (Normals.Num() < Verts.Num())
		{
			const TArray<FVector> Mine = ComputeNormals(false);
			for (int32 i = Normals.Num(); i < Verts.Num(); ++i) Normals.Add(Mine[i]);
		}
		const int32 Base = Verts.Num();
		const TArray<FVector> Theirs = O.Normals.Num() == O.Verts.Num() ? O.Normals : O.ComputeNormals(false);
		Verts.Append(O.Verts); Colors.Append(O.Colors); UVs.Append(O.UVs); Normals.Append(Theirs);
		for (int32 T : O.Tris) Tris.Add(Base + T);
	}

	/** Flat fan cap over a closed ring. */
	void Cap(const TArray<FVector>& Ring, const FLinearColor& C)
	{
		FVector Centroid = FVector::ZeroVector;
		for (const FVector& P : Ring) Centroid += P;
		Centroid /= (float)FMath::Max(1, Ring.Num());
		const int32 CI = AddVert(Centroid, C);
		const int32 First = Verts.Num();
		for (const FVector& P : Ring) AddVert(P, C);
		for (int32 k = 0; k < Ring.Num(); ++k) Tri(CI, First + k, First + (k + 1) % Ring.Num());
	}

	void Commit(UProceduralMeshComponent* Mesh, int32 Section, bool bCollision) const
	{
		TArray<FVector> Out = ComputeNormals(false);
		for (int32 i = 0; i < Normals.Num() && i < Out.Num(); ++i) Out[i] = Normals[i];
		Mesh->CreateMeshSection_LinearColor(Section, Verts, Tris, Out, UVs, Colors, TArray<FProcMeshTangent>(), bCollision);
	}
};

/** Rounded-box cross-section in the Y-Z plane at X. */
inline TArray<FVector> SuperRing(float X, float ZBottom, float ZTop, float HalfWBottom, float HalfWTop, int32 Points = 28, float N = 5.f)
{
	TArray<FVector> Ring;
	const float CZ = (ZBottom + ZTop) * 0.5f;
	const float HH = FMath::Max(0.5f, (ZTop - ZBottom) * 0.5f);
	for (int32 k = 0; k < Points; ++k)
	{
		const float A = 2.f * PI * k / Points;
		const float C = FMath::Cos(A), S = FMath::Sin(A);
		const float EY = FMath::Sign(C) * FMath::Pow(FMath::Abs(C), 2.f / N);
		const float EZ = FMath::Sign(S) * FMath::Pow(FMath::Abs(S), 2.f / N);
		const float HW = FMath::Lerp(HalfWBottom, HalfWTop, (EZ + 1.f) * 0.5f);
		Ring.Add(FVector(X, EY * HW, CZ + EZ * HH));
	}
	return Ring;
}
