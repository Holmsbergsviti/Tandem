#include "DriftCarMesh.h"
#include "Util/ProcGeo.h"
#include "ProceduralMeshComponent.h"

// Proportions follow the 1983-87 AE86 Sprinter Trueno GT-APEX 3-door hatch:
// 4205 x 1625 x 1335 mm, 2400 mm wheelbase, ~1355 mm track, 185/60R14 tyres.
// No badges or logos are modelled.

namespace
{
	const FLinearColor Glass(0.04f, 0.06f, 0.08f);
	const FLinearColor Trim(0.018f, 0.018f, 0.02f);   // black plastic and the lower two-tone
	const FLinearColor Liner(0.006f, 0.006f, 0.006f); // wheel wells, underside, shut lines
	const FLinearColor Chrome(0.55f, 0.55f, 0.58f);
	const FLinearColor TyreBlack(0.012f, 0.012f, 0.012f);
	const FLinearColor PlateWhite(0.5f, 0.5f, 0.46f);

	constexpr float GZ(float Height) { return DriftCarMesh::GroundZ + Height; }

	/** What differs between the car bodies in the shared loft code. */
	struct FStyle
	{
		float SplitZ;                     // two-tone line: paint above, black below
		float CabinFrontX, CabinBackX;    // where the lower body is left open for the greenhouse
		float ShelfFrontX, ShelfBackX;    // dark parcel shelf under the hatch glass
		float HingeY;
		bool bNSX;
	};
	const FStyle StyleAE86 = { GZ(49.f), 56.9f, -89.f, -95.f, -199.f, DriftCarMesh::DoorHingeY, false };
	const FStyle StyleNSX = { GZ(20.f), 56.9f, -89.f, 0.f, 0.f, DriftCarMesh::DoorHingeYNSX, true };
	const FStyle* Style = &StyleAE86;
	FVector Hinge(int32 Side) { return FVector(DriftCarMesh::DoorHingeX, Side == 0 ? -Style->HingeY : Style->HingeY, DriftCarMesh::DoorHingeZ); }

	void Box(FProcGeo& G, float X, float Y, float Z, float HX, float HY, float HZ, const FLinearColor& C, const FRotator& R = FRotator::ZeroRotator)
	{
		G.AddBox(FVector(X, Y, Z), FVector(HX, HY, HZ), C, FQuat(R));
	}

	/** Body side half-width at height Z: slight tumblehome above the shoulder, tuck-under at the sill. */
	float SideHW(float HW, float Z)
	{
		float Y = HW;
		if (Z > GZ(70.f)) Y -= (Z - GZ(70.f)) * 0.12f;
		if (Z < GZ(30.f)) Y -= (GZ(30.f) - Z) * 0.18f;
		return Y;
	}

	/** Closed ring from a right-hand half profile (y, z) that runs bottom centre -> top centre. */
	TArray<FVector> Mirror(float X, const TArray<FVector2D>& Half)
	{
		TArray<FVector> Ring;
		for (const FVector2D& P : Half) Ring.Add(FVector(X, P.X, P.Y));
		for (int32 j = Half.Num() - 2; j >= 1; --j) Ring.Add(FVector(X, -Half[j].X, Half[j].Y));
		return Ring;
	}

	struct FRingDef { float X, ZB, ZT, HW; };

	// Doors: front edge at the hinge, rear edge at X = -62 (glass stops at the B-pillar, -58).
	constexpr float DoorBackX = -62.f;
	constexpr float DoorGlassBackX = -58.f;

	/** Geometry routed into the two door meshes, stored relative to each door's hinge. */
	struct FDoorOut
	{
		FProcGeo Paint[2];
		FProcGeo Glass[2];
		void AddQuad(bool bGlass, const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3,
			const FLinearColor& C0, const FLinearColor& C1, const FLinearColor& C2, const FLinearColor& C3)
		{
			const int32 Side = (P0.Y + P2.Y) > 0.f ? 1 : 0;
			const FVector H = Hinge(Side);
			FProcGeo& G = bGlass ? Glass[Side] : Paint[Side];
			G.Quad(G.AddVert(P0 - H, C0), G.AddVert(P1 - H, C1), G.AddVert(P2 - H, C2), G.AddVert(P3 - H, C3));
		}
	};

	bool InDoorPanel(const FVector& Avg)
	{
		return Avg.X < DriftCarMesh::DoorHingeX && Avg.X > DoorBackX && FMath::Abs(Avg.Y) > 60.f && Avg.Z > GZ(21.f);
	}

	/** Lower body cross-section: flat floor, rounded sill, near-vertical side, rounded shoulder, flat top.
	 *  Corner radii and the side shape use absolute heights, so sections with different floors line up. */
	TArray<FVector> BodyRing(const FRingDef& D)
	{
		const float H = FMath::Max(1.f, D.ZT - D.ZB);
		const float RB = FMath::Min(5.f, H * 0.3f);
		const float RT = FMath::Min(9.f, H * 0.35f);
		const float ZS0 = D.ZB + RB, ZS1 = D.ZT - RT;
		const float YB = SideHW(D.HW, ZS0), YT = SideHW(D.HW, ZS1);
		TArray<FVector2D> Half;
		Half.Add(FVector2D(0.f, D.ZB));
		Half.Add(FVector2D((YB - RB) * 0.5f, D.ZB));
		Half.Add(FVector2D(YB - RB, D.ZB));
		for (float A : { 30.f, 60.f })
		{
			const float Rad = FMath::DegreesToRadians(A);
			Half.Add(FVector2D(YB - RB + RB * FMath::Sin(Rad), ZS0 - RB * FMath::Cos(Rad)));
		}
		Half.Add(FVector2D(YB, ZS0));
		// Two points 6 mm apart on the two-tone line keep the colour edge crisp.
		float Prev = ZS0;
		for (float Z : { Style->SplitZ - 0.3f, Style->SplitZ + 0.3f, (Style->SplitZ + ZS1) * 0.5f })
		{
			const float ZC = FMath::Clamp(FMath::Max(Z, Prev), ZS0, ZS1);
			Prev = ZC;
			Half.Add(FVector2D(SideHW(D.HW, ZC), ZC));
		}
		Half.Add(FVector2D(YT, ZS1));
		for (float A : { 22.5f, 45.f, 67.5f })
		{
			const float Rad = FMath::DegreesToRadians(A);
			Half.Add(FVector2D(YT - RT + RT * FMath::Cos(Rad), ZS1 + RT * FMath::Sin(Rad)));
		}
		Half.Add(FVector2D(YT - RT, D.ZT));
		Half.Add(FVector2D((YT - RT) * 0.5f, D.ZT));
		Half.Add(FVector2D(0.f, D.ZT));
		return Mirror(D.X, Half);
	}

	/** Smooth loft of one body section. bOpenCabin leaves the top open over the cabin. */
	void LoftBody(FProcGeo& G, const TArray<FRingDef>& Defs, const FLinearColor& Paint, bool bCapFront, bool bCapBack, bool bOpenCabin,
		FDoorOut* Doors = nullptr)
	{
		TArray<TArray<FVector>> Rings;
		for (const FRingDef& D : Defs) Rings.Add(BodyRing(D));
		const int32 M = Rings[0].Num();
		const int32 Base = G.Verts.Num();
		for (int32 r = 0; r < Rings.Num(); ++r)
		{
			for (int32 k = 0; k < M; ++k)
			{
				const FVector& P = Rings[r][k];
				const bool bUnder = P.Z < Defs[r].ZB + 0.5f && FMath::Abs(P.Y) < Defs[r].HW - 10.f;
				const bool bShelf = P.Z > Defs[r].ZT - 0.5f && P.X < Style->ShelfFrontX && P.X > Style->ShelfBackX && FMath::Abs(P.Y) < Defs[r].HW - 9.f;
				G.AddVert(P, (bUnder || bShelf) ? Liner : (P.Z >= Style->SplitZ ? Paint : Trim));
			}
		}
		for (int32 r = 0; r + 1 < Rings.Num(); ++r)
		{
			const float TopZ = (Defs[r].ZT + Defs[r + 1].ZT) * 0.5f;
			for (int32 k = 0; k < M; ++k)
			{
				const int32 K1 = (k + 1) % M;
				const FVector Avg = (Rings[r][k] + Rings[r + 1][k] + Rings[r + 1][K1] + Rings[r][K1]) * 0.25f;
				if (bOpenCabin && Avg.Z > TopZ - 0.05f && Avg.X < Style->CabinFrontX && Avg.X > Style->CabinBackX && FMath::Abs(Avg.Y) < Defs[r].HW - 9.f) continue;
				const int32 I0 = Base + r * M + k, I1 = Base + (r + 1) * M + k, I2 = Base + (r + 1) * M + K1, I3 = Base + r * M + K1;
				if (Doors && InDoorPanel(Avg))
				{
					Doors->AddQuad(false, G.Verts[I0], G.Verts[I1], G.Verts[I2], G.Verts[I3], G.Colors[I0], G.Colors[I1], G.Colors[I2], G.Colors[I3]);
					continue;
				}
				G.Quad(I0, I1, I2, I3);
			}
		}
		if (bCapFront) G.Cap(Rings[0], Trim);
		if (bCapBack) G.Cap(Rings.Last(), Trim);
	}

	/** Round wheel arch: painted panel that turns the square gap between body sections into an arch,
	 *  plus a dark liner and back wall so the wheel well looks solid. */
	void AddArch(FProcGeo& G, float XA, float HalfLen, float ZTop, float HW, const FLinearColor& Paint)
	{
		const float ZC = GZ(29.f), ZBot = GZ(17.f);
		const float R = ZTop - ZC;
		const float A0 = FMath::Asin(FMath::Clamp((ZBot - ZC) / R, -1.f, 1.f));
		constexpr int32 N = 28;
		TArray<FVector2D> Circle, Outer; // (x, z)
		for (int32 i = 0; i <= N; ++i)
		{
			const float A = FMath::Lerp(A0, PI - A0, (float)i / N);
			const float DX = FMath::Cos(A), DZ = FMath::Sin(A);
			float T = 1.e6f;
			if (DX > 1.e-3f) T = FMath::Min(T, HalfLen / DX);
			if (DX < -1.e-3f) T = FMath::Min(T, HalfLen / -DX);
			if (DZ > 1.e-3f) T = FMath::Min(T, (ZTop - ZC) / DZ);
			if (DZ < -1.e-3f) T = FMath::Min(T, (ZBot - ZC) / DZ);
			T = FMath::Max(T, R);
			Circle.Add(FVector2D(XA + DX * R, ZC + DZ * R));
			Outer.Add(FVector2D(XA + DX * T, ZC + DZ * T));
		}
		for (int32 S = -1; S <= 1; S += 2)
		{
			TArray<FVector> In, Out, Arc, Edge;
			for (int32 i = 0; i <= N; ++i)
			{
				Arc.Add(FVector(Circle[i].X, S * (SideHW(HW, Circle[i].Y) + 0.05f), Circle[i].Y));
				Edge.Add(FVector(Outer[i].X, S * (SideHW(HW, Outer[i].Y) + 0.05f), Outer[i].Y));
				const FVector2D L = FVector2D(XA, ZC) + (Circle[i] - FVector2D(XA, ZC)) * ((R + 0.5f) / R);
				Out.Add(FVector(L.X, S * (HW - 1.f), L.Y));
				In.Add(FVector(L.X, S * (HW - 30.f), L.Y));
			}
			G.Ribbon(Arc, Edge, [&](int32 i) { return Arc[i].Z >= Style->SplitZ ? Paint : Trim; });
			G.Ribbon(Out, In, [](int32) { return Liner; });
			const int32 CI = G.AddVert(FVector(XA, S * (HW - 30.f), ZC), Liner);
			const int32 First = G.Verts.Num();
			for (const FVector& P : In) G.AddVert(P, Liner);
			for (int32 i = 0; i < N; ++i) G.Tri(CI, First + i, First + i + 1);
			G.Tri(CI, First + N, First);
		}
	}

	// ---- Greenhouse (glass, pillars, roof) ----
	struct FGreenDef { float X, ZB, ZT, HWB, HWT; };
	enum class EGZone : uint8 { Bottom, Seal, Glass, Frame, Arc, Top };

	/** Half profile: bottom, seal, two glass bands, frame, rounded roof edge, flat top. */
	TArray<FVector> GreenRing(const FGreenDef& D)
	{
		const float H = FMath::Max(0.5f, D.ZT - D.ZB);
		const float RT = FMath::Min(9.f, H * 0.35f);
		const float ZS1 = D.ZT - RT;
		auto SideY = [&D, ZS1](float Z) { return FMath::Lerp(D.HWB, D.HWT, FMath::Clamp((Z - D.ZB) / FMath::Max(0.1f, ZS1 - D.ZB), 0.f, 1.f)); };
		const float Z2 = FMath::Min(D.ZB + 4.f, ZS1);
		const float Z4 = FMath::Max(Z2, ZS1 - 2.5f);
		TArray<FVector2D> Half;
		Half.Add(FVector2D(0.f, D.ZB));
		Half.Add(FVector2D(D.HWB, D.ZB));
		for (float Z : { Z2, (Z2 + Z4) * 0.5f, Z4, ZS1 }) Half.Add(FVector2D(SideY(Z), Z));
		for (float A : { 22.5f, 45.f, 67.5f })
		{
			const float Rad = FMath::DegreesToRadians(A);
			Half.Add(FVector2D(D.HWT - RT + RT * FMath::Cos(Rad), ZS1 + RT * FMath::Sin(Rad)));
		}
		Half.Add(FVector2D(D.HWT - RT, D.ZT));
		Half.Add(FVector2D((D.HWT - RT) * 0.5f, D.ZT));
		Half.Add(FVector2D(0.f, D.ZT));
		return Mirror(D.X, Half);
	}

	EGZone ZoneOf(int32 HalfSeg)
	{
		if (HalfSeg == 0) return EGZone::Bottom;
		if (HalfSeg == 1) return EGZone::Seal;
		if (HalfSeg <= 3) return EGZone::Glass;
		if (HalfSeg == 4) return EGZone::Frame;
		if (HalfSeg <= 8) return EGZone::Arc;
		return EGZone::Top;
	}

	bool InSideWindow(float X)
	{
		if (Style->bNSX) return (X < 54.f && X > -58.f) || (X < -68.f && X > -100.f);
		return (X < 54.f && X > -58.f) || (X < -68.f && X > -165.f);
	}
	bool InBPillar(float X) { return X <= -58.f && X >= -68.f; }

	/** -1 skip, 0 paint, 1 black trim, 2 glass. */
	int32 Classify(EGZone Z, float X)
	{
		if (Style->bNSX)
		{
			// Cab-forward canopy: black roof and pillars, glass all round, buttresses in body colour.
			switch (Z)
			{
			case EGZone::Seal:  return 1;
			case EGZone::Glass: return InSideWindow(X) ? 2 : 1;
			case EGZone::Frame: return 1;
			case EGZone::Arc:   return X > -100.f ? 1 : 0;
			case EGZone::Top:
				if (X > 78.f) return 1;   // cowl
				if (X > -2.f) return 2;   // windscreen
				if (X > -68.f) return 1;  // black roof
				if (X > -128.f) return 2; // rear glass
				return 0;                 // engine cover
			default: return -1;
			}
		}
		switch (Z)
		{
		case EGZone::Seal:  return 1;
		case EGZone::Glass: return InSideWindow(X) ? 2 : (InBPillar(X) ? 1 : 0);
		case EGZone::Frame: return (InSideWindow(X) || InBPillar(X)) ? 1 : 0;
		case EGZone::Arc:   return X < -197.f ? 1 : 0;
		case EGZone::Top:
			if (X > 55.f) return 1;   // cowl
			if (X > -3.f) return 2;   // windscreen
			if (X > -105.f) return 0; // roof
			if (X > -197.f) return 2; // hatch glass
			return 1;                 // hatch lip
		default: return -1;
		}
	}

	const FGreenDef GreenNSX[] = {
		{  84.f, GZ(76.5f),  GZ(79.f),    84.f, 83.f },
		{  76.f, GZ(77.5f),  GZ(85.f),    84.f, 81.f },
		{  56.f, GZ(80.5f),  GZ(98.f),    84.f, 75.f },
		{  36.f, GZ(83.f),   GZ(109.f),   84.f, 69.f },
		{  16.f, GZ(84.5f),  GZ(118.f),   84.f, 64.f },
		{  -4.f, GZ(85.5f),  GZ(124.f),   84.f, 61.5f },
		{ -25.f, GZ(86.f),   GZ(126.f),   84.f, 60.5f },
		{ -58.f, GZ(86.5f),  GZ(126.f),   84.f, 60.f },
		{ -68.f, GZ(86.5f),  GZ(125.5f),  84.f, 60.f },
		{ -92.f, GZ(87.f),   GZ(119.f),   84.f, 61.f },
		{ -115.f, GZ(88.f),  GZ(108.f),   84.f, 63.f },
		{ -140.f, GZ(89.f),  GZ(99.f),    84.f, 67.f },
		{ -160.f, GZ(89.8f), GZ(93.f),    83.f, 71.f },
		{ -168.f, GZ(90.f),  GZ(91.f),    82.f, 74.f },
	};

	void BuildGreenhouse(FProcGeo& PaintOut, FProcGeo& GlassOut, const FLinearColor& Paint, FDoorOut* Doors)
	{
		const FGreenDef DefsAE86[] = {
			{  58.f, GZ(82.f),   GZ(86.f),    77.f, 76.f },
			{  53.f, GZ(82.5f),  GZ(89.6f),   77.f, 74.6f },
			{  40.f, GZ(84.f),   GZ(99.f),    77.f, 71.f },
			{  20.f, GZ(85.f),   GZ(114.f),   77.f, 65.f },
			{   0.f, GZ(85.5f),  GZ(127.f),   77.f, 61.f },
			{ -15.f, GZ(86.f),   GZ(131.5f),  77.f, 59.5f },
			{ -35.f, GZ(86.f),   GZ(133.f),   77.f, 59.f },
			{ -58.f, GZ(86.5f),  GZ(133.5f),  77.f, 58.5f },
			{ -68.f, GZ(86.5f),  GZ(133.5f),  77.f, 58.5f },
			{ -100.f, GZ(87.f),  GZ(131.5f),  77.f, 58.5f },
			{ -125.f, GZ(87.f),  GZ(125.f),   77.f, 59.f },
			{ -150.f, GZ(87.f),  GZ(116.f),   77.f, 61.f },
			{ -175.f, GZ(87.5f), GZ(105.f),   76.f, 64.f },
			{ -198.f, GZ(88.f),  GZ(94.f),    74.f, 68.f },
			{ -206.f, GZ(87.f),  GZ(90.5f),   72.f, 70.f },
		};
		TArray<TArray<FVector>> Rings;
		if (Style->bNSX) { for (const FGreenDef& D : GreenNSX) Rings.Add(GreenRing(D)); }
		else { for (const FGreenDef& D : DefsAE86) Rings.Add(GreenRing(D)); }
		const int32 M = Rings[0].Num();
		const int32 HalfN = M / 2 + 1;
		auto HalfIdx = [M, HalfN](int32 Idx) { return Idx < HalfN ? Idx : M - Idx; };
		for (int32 r = 0; r + 1 < Rings.Num(); ++r)
		{
			for (int32 k = 0; k < M; ++k)
			{
				const int32 K1 = (k + 1) % M;
				const FVector P0 = Rings[r][k], P1 = Rings[r + 1][k], P2 = Rings[r + 1][K1], P3 = Rings[r][K1];
				const EGZone Zone = ZoneOf(FMath::Min(HalfIdx(k), HalfIdx(K1)));
				const float AvgX = (P0.X + P1.X) * 0.5f;
				const int32 Cls = Classify(Zone, AvgX);
				if (Cls < 0) continue;
				// Door window, its seal and frame move with the door.
				if (Doors && (Zone == EGZone::Seal || Zone == EGZone::Glass || Zone == EGZone::Frame)
					&& AvgX < DriftCarMesh::DoorHingeX && AvgX > DoorGlassBackX && Cls != 0)
				{
					const FLinearColor DC = Cls == 2 ? Glass : (Cls == 1 ? Trim : Paint);
					Doors->AddQuad(Cls == 2, P0, P1, P2, P3, DC, DC, DC, DC);
					continue;
				}
				FProcGeo& T = Cls == 2 ? GlassOut : PaintOut;
				const FLinearColor C = Cls == 2 ? Glass : (Cls == 1 ? Trim : Paint);
				T.Quad(T.AddVert(P0, C), T.AddVert(P1, C), T.AddVert(P2, C), T.AddVert(P3, C));
			}
		}
		PaintOut.Cap(Rings[0], Trim);
		PaintOut.Cap(Rings.Last(), Trim);
		PaintOut.BakeNormals(true);
		GlassOut.BakeNormals(true);
	}
}

namespace
{
	/** Door parts that aren't body panels (mirror, handle, inner door card with armrest), built in body
	 *  space and shifted to the hinge, then both doors committed. */
	void FinishDoors(FDoorOut& DoorGeo, UProceduralMeshComponent* DoorLeft, UProceduralMeshComponent* DoorRight, float HW)
	{
		if (!DoorLeft || !DoorRight) return;
		const float Out = HW - 81.f; // the AE86 parts sit at HW 81; wider bodies move them out
		FProcGeo Door[2];
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float S = Side == 0 ? -1.f : 1.f;
			FProcGeo& D = Door[Side];
			Box(D, 47.f, S * (84.f + Out), GZ(95.f), 4.f, 5.f, 4.f, Trim);                                  // mirror
			Box(D, 50.f, S * (79.5f + Out), GZ(92.f), 3.f, 2.5f, 1.2f, Trim);
			Box(D, -48.f, S * (SideHW(HW, GZ(80.f)) + 0.3f), GZ(80.f), 5.f, 0.5f, 1.f, Liner);                // handle
			Box(D, -4.f, S * (77.f + Out), GZ(57.f), 56.f, 1.2f, 31.f, FLinearColor(0.03f, 0.03f, 0.032f));   // door card
			Box(D, -20.f, S * (74.f + Out), GZ(70.f), 18.f, 2.5f, 2.f, FLinearColor(0.025f, 0.025f, 0.028f)); // armrest
			const FVector H = Hinge(Side);
			for (FVector& V : D.Verts) V -= H;
		}
		UProceduralMeshComponent* DoorMeshes[2] = { DoorLeft, DoorRight };
		for (int32 Side = 0; Side < 2; ++Side)
		{
			DoorMeshes[Side]->ClearAllMeshSections();
			DoorGeo.Paint[Side].BakeNormals(true);
			DoorGeo.Paint[Side].Append(Door[Side]);
			DoorGeo.Paint[Side].Commit(DoorMeshes[Side], 0, false);
			DoorGeo.Glass[Side].BakeNormals(true);
			DoorGeo.Glass[Side].Commit(DoorMeshes[Side], 1, false);
		}
	}

	/** Honda NSX (NA1) style: 4405 x 1810 x 1170 mm (a little taller here so the shared cabin fits),
	 *  2530 mm wheelbase, front axle at X +126, rear at X -127. */
	void BuildNSXBody(UProceduralMeshComponent* Mesh, const FLinearColor& Paint, FDoorOut* Doors)
	{
		constexpr float HW = 88.f;
		const float ArchTop = GZ(64.f);
		FProcGeo G;
		// Nose: low pointed wedge.
		LoftBody(G, {
			{ 222.f, GZ(22.f), GZ(40.f), 70.f }, { 219.f, GZ(18.f), GZ(50.f), 80.f }, { 212.f, GZ(16.f), GZ(57.f), 85.f },
			{ 200.f, GZ(15.f), GZ(61.5f), 87.5f }, { 182.f, GZ(15.f), GZ(64.5f), HW }, { 164.f, GZ(15.f), GZ(66.5f), HW } },
			Paint, true, true, false);
		// Front fenders over the arch (front axle at +126).
		LoftBody(G, {
			{ 164.f, ArchTop, GZ(66.5f), HW }, { 145.f, ArchTop, GZ(69.f), HW }, { 126.f, ArchTop, GZ(71.f), HW },
			{ 107.f, ArchTop, GZ(73.5f), HW }, { 88.f, ArchTop, GZ(76.f), HW } }, Paint, false, false, false);
		// Doors and cabin.
		LoftBody(G, {
			{ 88.f, GZ(15.f), GZ(76.f), HW }, { 76.f, GZ(15.f), GZ(77.5f), HW }, { 57.f, GZ(15.f), GZ(80.5f), HW },
			{ 53.f, GZ(15.f), GZ(81.f), HW }, { 36.f, GZ(15.f), GZ(83.f), HW }, { 0.f, GZ(15.f), GZ(85.5f), HW },
			{ -45.f, GZ(15.f), GZ(86.2f), HW }, { -62.f, GZ(15.f), GZ(86.5f), HW }, { -89.f, GZ(15.f), GZ(87.f), HW } },
			Paint, true, true, true, Doors);
		// Rear quarters over the arch (rear axle at -127).
		LoftBody(G, {
			{ -89.f, ArchTop, GZ(87.f), HW }, { -108.f, ArchTop, GZ(88.f), HW }, { -127.f, ArchTop, GZ(89.f), HW },
			{ -146.f, ArchTop, GZ(89.6f), HW }, { -165.f, ArchTop, GZ(90.f), HW } }, Paint, false, false, false);
		// Tail: long deck down to the lamp bar.
		LoftBody(G, {
			{ -165.f, GZ(15.f), GZ(90.f), HW }, { -185.f, GZ(16.f), GZ(90.3f), 87.5f }, { -200.f, GZ(18.f), GZ(90.f), 86.5f },
			{ -212.f, GZ(21.f), GZ(88.f), 84.f }, { -219.f, GZ(26.f), GZ(84.f), 80.f }, { -222.f, GZ(32.f), GZ(79.f), 74.f } },
			Paint, true, true, false);
		AddArch(G, 126.f, 38.f, ArchTop, HW, Paint);
		AddArch(G, -127.f, 38.f, ArchTop, HW, Paint);

		// Bumpers, lip, plates, sills, side intakes, spoiler, exhausts.
		// (Bumpers and the spoiler are separate parts that can be knocked off: see BuildLoosePart.)
		for (int32 S = -1; S <= 1; S += 2)
		{
			Box(G, -224.f, S * 45.f, GZ(26.f), 4.f, 4.f, 3.f, Chrome);
			Box(G, -224.f, S * 55.f, GZ(26.f), 4.f, 4.f, 3.f, Chrome);
			Box(G, 0.f, S * (HW - 1.f), GZ(17.f), 85.f, 1.5f, 2.5f, Trim);                        // sill
			Box(G, -78.f, S * (SideHW(HW, GZ(66.f)) + 0.2f), GZ(66.f), 10.f, 0.6f, 9.f, Liner);   // side intake
			Box(G, -80.f, S * (SideHW(HW, GZ(66.f)) - 1.5f), GZ(66.f), 8.f, 2.f, 7.f, Trim);
			// Pop-up headlight lids (closed) with a shut line.
			Box(G, 186.f, S * 56.f, GZ(64.6f), 11.6f, 17.6f, 0.4f, Liner, FRotator(-8.f, 0.f, 0.f));
			Box(G, 186.f, S * 56.f, GZ(64.9f), 11.f, 17.f, 0.5f, Paint, FRotator(-8.f, 0.f, 0.f));
			for (float DX : { 53.f, -62.f })
			{
				Box(G, DX, S * (HW + 0.1f), GZ(48.f), 0.25f, 0.3f, 22.f, Liner);
			}
		}
		FProcGeo L;
		const FLinearColor TailRed(0.14f, 0.003f, 0.0015f), Amber(0.06f, 0.022f, 0.f), Clear(0.25f, 0.25f, 0.24f);
		L.AddBox(FVector(-222.2f, 0.f, GZ(72.f)), FVector(1.f, 80.f, 4.5f), TailRed);  // full-width lamp bar
		for (int32 S = -1; S <= 1; S += 2)
		{
			L.AddBox(FVector(221.f, S * 56.f, GZ(36.f)), FVector(1.f, 14.f, 2.5f), Clear);  // driving lamps
			L.AddBox(FVector(219.f, S * 76.f, GZ(36.f)), FVector(1.f, 5.f, 2.5f), Amber);
			L.AddBox(FVector(-222.4f, S * 60.f, GZ(72.f)), FVector(1.f, 8.f, 2.f), Amber);
		}
		FProcGeo GH, Gl;
		BuildGreenhouse(GH, Gl, Paint, Doors);
		G.Append(GH);
		G.Commit(Mesh, 0, false);
		Gl.Commit(Mesh, 2, false);
		L.Commit(Mesh, 1, false);
	}
}

void DriftCarMesh::BuildBody(UProceduralMeshComponent* Mesh, const FLinearColor& Paint,
	UProceduralMeshComponent* DoorLeft, UProceduralMeshComponent* DoorRight, int32 Model)
{
	if (!Mesh) return;
	Mesh->ClearAllMeshSections();
	Style = Model == 1 ? &StyleNSX : &StyleAE86;
	const bool bDoors = DoorLeft && DoorRight;
	FDoorOut DoorGeo;
	FDoorOut* Doors = bDoors ? &DoorGeo : nullptr;
	if (Model == 1)
	{
		BuildNSXBody(Mesh, Paint, Doors);
		FinishDoors(DoorGeo, DoorLeft, DoorRight, 88.f);
		Style = &StyleAE86;
		return;
	}

	constexpr float HW = 81.f;
	const float ArchTop = GZ(65.f);
	FProcGeo G;

	// Nose: wrap-around bumper area and the low wedge under the pop-up lamps.
	LoftBody(G, {
		{ 211.f, GZ(24.f), GZ(48.f), 70.f }, { 209.f, GZ(19.f), GZ(58.f), 76.f }, { 205.f, GZ(17.f), GZ(64.f), 79.f },
		{ 198.f, GZ(16.f), GZ(67.f), 80.5f }, { 185.f, GZ(16.f), GZ(69.5f), HW }, { 170.f, GZ(16.f), GZ(71.5f), HW },
		{ 152.f, GZ(17.f), GZ(73.5f), HW } }, Paint, true, true, false);
	// Front fenders over the arch.
	LoftBody(G, {
		{ 152.f, ArchTop, GZ(73.5f), HW }, { 134.f, ArchTop, GZ(75.f), HW }, { 115.f, ArchTop, GZ(76.5f), HW },
		{ 96.f, ArchTop, GZ(78.f), HW }, { 78.f, ArchTop, GZ(79.5f), HW } }, Paint, false, false, false);
	// Doors and cabin (open on top, the greenhouse sits on it).
	LoftBody(G, {
		{ 78.f, GZ(17.f), GZ(79.5f), HW }, { 66.f, GZ(17.f), GZ(81.f), HW }, { 57.f, GZ(17.f), GZ(82.5f), HW },
		{ 53.f, GZ(17.f), GZ(83.2f), HW }, { 40.f, GZ(17.f), GZ(85.5f), HW }, { 0.f, GZ(17.f), GZ(87.f), HW },
		{ -45.f, GZ(17.f), GZ(88.f), HW }, { -62.f, GZ(17.f), GZ(88.4f), HW }, { -89.f, GZ(17.f), GZ(89.f), HW } },
		Paint, true, true, true, Doors);
	// Rear quarters over the arch.
	LoftBody(G, {
		{ -89.f, ArchTop, GZ(89.f), HW }, { -107.f, ArchTop, GZ(89.5f), HW }, { -125.f, ArchTop, GZ(90.f), HW },
		{ -143.f, ArchTop, GZ(90.f), HW }, { -161.f, ArchTop, GZ(90.f), HW } }, Paint, false, false, false);
	// Tail.
	LoftBody(G, {
		{ -161.f, GZ(17.f), GZ(90.f), HW }, { -180.f, GZ(18.f), GZ(90.5f), 80.5f }, { -195.f, GZ(20.f), GZ(90.5f), 79.5f },
		{ -204.f, GZ(23.f), GZ(89.f), 77.5f }, { -209.f, GZ(27.f), GZ(86.f), 74.f }, { -211.f, GZ(32.f), GZ(82.f), 70.f } },
		Paint, true, true, false);

	AddArch(G, 115.f, 37.f, ArchTop, HW, Paint);
	AddArch(G, -125.f, 36.f, ArchTop, HW, Paint);

	// Bumpers, front lip, plates.
	// (Bumpers and the spoiler are separate parts that can be knocked off: see BuildLoosePart.)
	// Nose grille strip and the tail-panel garnish between the lamps.
	Box(G, 210.4f, 0.f, GZ(53.f), 0.8f, 44.f, 2.2f, Liner);
	Box(G, -211.4f, 0.f, GZ(73.f), 0.9f, 32.f, 8.f, Liner);
	// Hatch lip spoiler and exhaust.
	Box(G, -212.f, -48.f, GZ(29.f), 5.f, 3.5f, 3.f, Chrome);

	FProcGeo L;
	const FLinearColor TailRed(0.14f, 0.003f, 0.0015f), Amber(0.06f, 0.022f, 0.f); // kept low: bright reds tone-map to orange
	for (int32 S = -1; S <= 1; S += 2)
	{
		// Pop-up headlight lids (closed) with a dark shut line around each.
		Box(G, 190.f, S * 57.f, GZ(68.5f), 9.6f, 16.6f, 0.4f, Liner, FRotator(-11.f, 0.f, 0.f));
		Box(G, 190.f, S * 57.f, GZ(68.8f), 9.f, 16.f, 0.5f, Paint, FRotator(-11.f, 0.f, 0.f));
		// Door shut lines (front and rear edge) and handle.
		for (float DX : { 53.f, -62.f })
		{
			Box(G, DX, S * 81.1f, GZ(50.f), 0.25f, 0.3f, 20.f, Liner);
			for (float H : { 73.f, 79.f, 85.f }) Box(G, DX, S * (SideHW(HW, GZ(H)) + 0.1f), GZ(H), 0.25f, 0.3f, 3.f, Liner);
		}
		// Lamps: amber corners in the nose, full-width red tail lamps.
		L.AddBox(FVector(210.2f, S * 58.f, GZ(53.f)), FVector(1.2f, 12.f, 2.6f), Amber);
		L.AddBox(FVector(-211.2f, S * 56.f, GZ(73.f)), FVector(1.f, 14.f, 8.f), TailRed);
		L.AddBox(FVector(-211.2f, S * 37.f, GZ(73.f)), FVector(1.f, 5.f, 8.f), Amber);
	}

	FProcGeo GH, Gl;
	BuildGreenhouse(GH, Gl, Paint, Doors);
	G.Append(GH);
	G.Commit(Mesh, 0, false);
	Gl.Commit(Mesh, 2, false);
	L.Commit(Mesh, 1, false);

	FinishDoors(DoorGeo, DoorLeft, DoorRight, HW);
}

void DriftCarMesh::BuildWheel(UProceduralMeshComponent* Mesh, float Radius, float HalfWidth)
{
	if (!Mesh) return;
	Mesh->ClearAllMeshSections();
	const float R = Radius, W = HalfWidth;
	const float RimR = R * 0.615f; // 14" rim inside a 185/60 tyre

	FProcGeo Tyre;
	Tyre.Revolve({ { RimR, -W + 0.8f }, { R - 2.8f, -W }, { R - 0.6f, -W + 2.2f }, { R, -W + 4.5f },
		{ R, W - 4.5f }, { R - 0.6f, W - 2.2f }, { R - 2.8f, W }, { RimR, W - 0.8f } }, 36, TyreBlack);
	Tyre.Commit(Mesh, 0, false);

	FProcGeo Rim;
	// Barrel with an outer lip, dark recessed centre, hub.
	Rim.Revolve({ { RimR - 1.6f, W - 0.4f }, { RimR + 0.4f, W - 0.2f }, { RimR + 0.4f, W - 2.f }, { RimR - 0.3f, W - 2.5f },
		{ RimR - 0.3f, -W + 1.f }, { RimR - 2.5f, -W + 1.f } }, 36, Chrome);
	Rim.Revolve({ { 0.f, W - 7.f }, { RimR - 0.3f, W - 7.f } }, 24, Liner);
	Rim.Revolve({ { 0.f, W - 1.2f }, { 3.f, W - 1.2f }, { 4.8f, W - 2.2f }, { 5.5f, W - 6.5f } }, 20, Chrome);
	// Eight straight spokes.
	const float Inner = 5.f, OuterR = RimR - 1.f;
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = 2.f * PI * i / 8.f;
		const float Mid = (Inner + OuterR) * 0.5f;
		Rim.AddBox(FVector(FMath::Cos(A) * Mid, W - 3.5f, FMath::Sin(A) * Mid), FVector((OuterR - Inner) * 0.5f, 1.4f, 1.5f), Chrome,
			FQuat(FVector::YAxisVector, -A));
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const float A = 2.f * PI * (i + 0.5f) / 4.f;
		Rim.AddBox(FVector(FMath::Cos(A) * 3.4f, W - 0.9f, FMath::Sin(A) * 3.4f), FVector(0.6f, 0.5f, 0.6f), Liner);
	}
	Rim.Commit(Mesh, 1, false);
}

namespace
{
	const FLinearColor Charcoal(0.025f, 0.025f, 0.028f);
	const FLinearColor Pad(0.012f, 0.012f, 0.013f);
	const FLinearColor Carpet(0.015f, 0.015f, 0.016f);
	const FLinearColor DoorCard(0.03f, 0.03f, 0.032f);
	const FLinearColor Seat(0.035f, 0.035f, 0.04f);
	const FLinearColor SeatRed(0.35f, 0.02f, 0.02f);
	const FLinearColor Glove(0.02f, 0.02f, 0.02f);
	const FLinearColor Sleeve(0.03f, 0.03f, 0.035f);
}

namespace CabinParts
{
	/** Eight-sided tube from A to B, faces pointing outwards. */
	void Tube(FProcGeo& G, const FVector& A, const FVector& B, float Rad, const FLinearColor& C)
	{
		const FVector Axis = (B - A).GetSafeNormal();
		if (Axis.IsNearlyZero()) return;
		const FVector Ref = FMath::Abs(Axis.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		const FVector U = FVector::CrossProduct(Ref, Axis).GetSafeNormal();
		const FVector V = FVector::CrossProduct(Axis, U);
		constexpr int32 Sides = 8;
		const int32 Base = G.Verts.Num();
		for (int32 k = 0; k < Sides; ++k)
		{
			const float Ang = 2.f * PI * k / Sides;
			const FVector Off = (U * FMath::Cos(Ang) + V * FMath::Sin(Ang)) * Rad;
			G.AddVert(A + Off, C);
			G.AddVert(B + Off, C);
		}
		for (int32 k = 0; k < Sides; ++k)
		{
			const int32 K1 = (k + 1) % Sides;
			G.Quad(Base + k * 2, Base + k * 2 + 1, Base + K1 * 2 + 1, Base + K1 * 2);
		}
	}

	/** Flat belt strap from A to B. */
	void Strap(FProcGeo& G, const FVector& A, const FVector& B, float HalfW, const FLinearColor& C)
	{
		const FVector Dir = B - A;
		G.AddBox((A + B) * 0.5f, FVector(Dir.Size() * 0.5f, HalfW, 0.25f), C, FQuat(Dir.Rotation()));
	}

	/** Round dial facing the driver (-X): dark face, Major numbered ticks plus half ticks, red from RedFrom. */
	void Dial(FProcGeo& G, const FVector& Ctr, float R, int32 Major, int32 RedFrom)
	{
		const FLinearColor Face(0.012f, 0.012f, 0.014f), Tick(0.85f, 0.85f, 0.8f), Red(0.9f, 0.05f, 0.03f);
		constexpr int32 Segs = 36;
		const FVector F = Ctr + FVector(0.5f, 0.f, 0.f);
		const int32 CI = G.AddVert(F, Face);
		const int32 First = G.Verts.Num();
		for (int32 k = 0; k < Segs; ++k)
		{
			const float Ang = 2.f * PI * k / Segs;
			G.AddVert(F + FVector(0.f, FMath::Cos(Ang) * R, FMath::Sin(Ang) * R), Face);
		}
		for (int32 k = 0; k < Segs; ++k) G.Tri(CI, First + k, First + (k + 1) % Segs);
		for (int32 m = 0; m <= Major * 2; ++m)
		{
			const float Frac = static_cast<float>(m) / (Major * 2);
			const float Phi = FMath::DegreesToRadians(-0.5f * DriftCarMesh::GaugeSweepDeg + DriftCarMesh::GaugeSweepDeg * Frac);
			const FVector2D Up(FMath::Sin(Phi), FMath::Cos(Phi)); // (Y, Z): 0 = straight up, + = clockwise to the driver
			const FVector2D Across(Up.Y, -Up.X);
			const bool bMajor = m % 2 == 0;
			const float R0 = R * (bMajor ? 0.72f : 0.82f), R1 = R * 0.95f, HW = R * (bMajor ? 0.045f : 0.025f);
			const FLinearColor TC = (m >= RedFrom * 2) ? Red : Tick;
			auto At = [&](float Rad, float Side) { const FVector2D Q = Up * Rad + Across * Side; return Ctr + FVector(0.2f, Q.X, Q.Y); };
			G.Quad(G.AddVert(At(R0, -HW), TC), G.AddVert(At(R1, -HW), TC), G.AddVert(At(R1, HW), TC), G.AddVert(At(R0, HW), TC));
		}
	}
}

void DriftCarMesh::BuildInterior(UProceduralMeshComponent* Mesh, float MirrorDrop)
{
	if (!Mesh) return;
	Mesh->ClearAllMeshSections();
	FProcGeo G;
	const float D = 38.f; // driver centre line, right-hand drive

	// Floor, door cards and rear trim cover the inside of the painted shell.
	// The cabin runs from the dash to the rear bulkhead at X = -89 (the rear arches start there).
	Box(G, -16.5f, 0.f, GZ(26.f), 72.f, 76.f, 1.f, Carpet);
	for (int32 S = -1; S <= 1; S += 2)
	{
		Box(G, -135.f, S * 76.5f, GZ(77.f), 60.f, 1.2f, 11.f, DoorCard); // above the rear arch
	}
	// Dashboard, top pad, binnacle over the gauges in front of the driver, centre stack, console.
	Box(G, 42.f, 0.f, GZ(75.f), 14.f, 76.f, 9.f, Charcoal);
	Box(G, 41.f, 0.f, GZ(84.5f), 11.f, 76.f, 1.2f, Pad);
	Box(G, 24.f, D, GZ(91.f), 4.f, 15.f, 2.5f, Pad);   // visor over the gauges
	Box(G, 25.f, D - 15.f, GZ(86.5f), 3.f, 0.8f, 6.f, Pad);
	Box(G, 25.f, D + 15.f, GZ(86.5f), 3.f, 0.8f, 6.f, Pad);
	// Instrument cluster: black panel with a rev counter (0-9, red from 8) and a speedometer (0-220 km/h).
	Box(G, 28.1f, D, GZ(81.f), 0.4f, 13.5f, 5.5f, Pad);
	FProcGeo Lit;
	CabinParts::Dial(Lit, DriftCarMesh::TachCentre(), 5.2f, 9, 8);
	CabinParts::Dial(Lit, DriftCarMesh::SpeedoCentre(), 5.2f, 11, 12);
	Box(Lit, 23.6f, 0.f, GZ(66.f), 0.4f, 8.f, 1.6f, FLinearColor(0.02f, 0.3f, 0.12f)); // radio display
	// Air vents.
	for (int32 S = -1; S <= 1; S += 2)
	{
		Box(G, 27.6f, S * 9.f, GZ(78.f), 0.5f, 5.f, 2.f, FLinearColor(0.06f, 0.06f, 0.065f));
		Box(G, 27.6f, S * 60.f, GZ(78.f), 0.5f, 6.f, 2.f, FLinearColor(0.06f, 0.06f, 0.065f));
	}
	// Rear-view mirror on the roof just behind the windscreen.
	Box(G, -6.f, 0.f, GZ(124.f) - MirrorDrop, 1.f, 1.f, 4.f, Charcoal);
	Box(G, -4.f, 0.f, GZ(119.f) - MirrorDrop, 1.2f, 11.f, 3.f, Charcoal);
	Box(G, -5.4f, 0.f, GZ(119.f) - MirrorDrop, 0.2f, 10.f, 2.4f, FLinearColor(0.35f, 0.37f, 0.4f));
	Box(G, 34.f, 0.f, GZ(64.f), 10.f, 14.f, 12.f, Charcoal);
	Box(G, -18.f, 0.f, GZ(38.f), 40.f, 12.f, 9.f, Charcoal);
	// Bucket seats with red bolsters (the bulkhead behind them doubles as the rear seat back).
	for (int32 S = -1; S <= 1; S += 2)
	{
		const float Y = D * S;
		Box(G, -48.f, Y, GZ(46.f), 22.f, 22.f, 5.f, Seat);
		Box(G, -74.f, Y, GZ(72.f), 5.f, 22.f, 24.f, Seat, FRotator(14.f, 0.f, 0.f));
		Box(G, -48.f, Y - 21.f, GZ(50.f), 22.f, 3.f, 6.f, SeatRed);
		Box(G, -48.f, Y + 21.f, GZ(50.f), 22.f, 3.f, 6.f, SeatRed);
		Box(G, -79.f, Y, GZ(101.f), 4.f, 12.f, 7.f, Seat, FRotator(14.f, 0.f, 0.f));
	}
	// Steering column.
	Box(G, 19.f, D, GZ(77.5f), 11.f, 2.5f, 2.5f, Liner, FRotator(-22.f, 0.f, 0.f));

	// Red 4-point harnesses lying on both bucket seats.
	const FLinearColor Belt(0.4f, 0.02f, 0.02f), Buckle(0.4f, 0.4f, 0.42f);
	for (int32 S = -1; S <= 1; S += 2)
	{
		const float Y = D * S;
		for (int32 B = -1; B <= 1; B += 2)
		{
			CabinParts::Strap(G, FVector(-87.f, Y + B * 9.f, 40.f), FVector(-76.f, Y + B * 8.f, 47.f), 2.5f, Belt);
			CabinParts::Strap(G, FVector(-76.f, Y + B * 8.f, 47.f), FVector(-63.f, Y + B * 9.f, 2.f), 2.5f, Belt);
			CabinParts::Strap(G, FVector(-58.f, Y + B * 20.f, -1.f), FVector(-46.f, Y + B * 5.f, -0.5f), 2.5f, Belt);
		}
		Box(G, -45.f, Y, GZ(51.f), 1.5f, 4.f, 3.f, Buckle);
	}

	// Bolted-in roll cage: main hoop behind the seats, legs up the A-pillars, roof rails, X door bars,
	// rear stays, harness bar and a diagonal.
	FProcGeo Cage;
	const FLinearColor CageC(0.28f, 0.28f, 0.3f);
	constexpr float CR = 2.f;
	for (int32 S = -1; S <= 1; S += 2)
	{
		CabinParts::Tube(Cage, FVector(-87.f, S * 64.f, -25.f), FVector(-87.f, S * 64.f, 30.f), CR, CageC);
		CabinParts::Tube(Cage, FVector(-87.f, S * 64.f, 30.f), FVector(-87.f, S * 54.f, 72.f), CR, CageC);
		CabinParts::Tube(Cage, FVector(40.f, S * 64.f, -25.f), FVector(40.f, S * 64.f, 25.f), CR, CageC);
		CabinParts::Tube(Cage, FVector(40.f, S * 64.f, 25.f), FVector(8.f, S * 56.f, 62.f), CR, CageC);
		CabinParts::Tube(Cage, FVector(8.f, S * 56.f, 62.f), FVector(-20.f, S * 54.f, 74.f), CR, CageC);
		CabinParts::Tube(Cage, FVector(-20.f, S * 54.f, 74.f), FVector(-87.f, S * 54.f, 72.f), CR, CageC);
		CabinParts::Tube(Cage, FVector(35.f, S * 67.f, -15.f), FVector(-85.f, S * 66.f, 22.f), CR, CageC);
		CabinParts::Tube(Cage, FVector(35.f, S * 67.f, 22.f), FVector(-85.f, S * 66.f, -15.f), CR, CageC);
		CabinParts::Tube(Cage, FVector(-87.f, S * 54.f, 72.f), FVector(-165.f, S * 52.f, 5.f), CR, CageC);
	}
	CabinParts::Tube(Cage, FVector(-87.f, -54.f, 72.f), FVector(-87.f, 54.f, 72.f), CR, CageC);
	CabinParts::Tube(Cage, FVector(-20.f, -54.f, 74.f), FVector(-20.f, 54.f, 74.f), CR, CageC);
	CabinParts::Tube(Cage, FVector(-87.f, -64.f, 38.f), FVector(-87.f, 64.f, 38.f), CR, CageC);
	CabinParts::Tube(Cage, FVector(-87.f, -64.f, -25.f), FVector(-87.f, 54.f, 72.f), CR, CageC);
	CabinParts::Tube(Cage, FVector(40.f, -64.f, 15.f), FVector(40.f, 64.f, 15.f), CR, CageC);

	G.Commit(Mesh, 0, false);
	Lit.Commit(Mesh, 1, false);
	Cage.Commit(Mesh, 2, false);
}

void DriftCarMesh::BuildNeedle(UProceduralMeshComponent* Mesh, float Length)
{
	if (!Mesh) return;
	Mesh->ClearAllMeshSections();
	FProcGeo G;
	const FLinearColor Orange(1.f, 0.35f, 0.02f), Hub(0.05f, 0.05f, 0.05f);
	G.Quad(G.AddVert(FVector(0.f, -0.22f, -0.8f), Orange), G.AddVert(FVector(0.f, -0.08f, Length), Orange),
		G.AddVert(FVector(0.f, 0.08f, Length), Orange), G.AddVert(FVector(0.f, 0.22f, -0.8f), Orange));
	G.AddBox(FVector(-0.1f, 0.f, 0.f), FVector(0.15f, 0.6f, 0.6f), Hub);
	G.Commit(Mesh, 0, false);
}

void DriftCarMesh::BuildSteeringWheel(UProceduralMeshComponent* Mesh)
{
	if (!Mesh) return;
	Mesh->ClearAllMeshSections();
	FProcGeo G;
	TArray<FVector2D> RimProfile;
	for (int32 i = 0; i <= 12; ++i)
	{
		const float T = 2.f * PI * i / 12.f;
		RimProfile.Add(FVector2D(17.5f + 1.7f * FMath::Cos(T), 1.7f * FMath::Sin(T)));
	}
	G.Revolve(RimProfile, 36, FLinearColor(0.015f, 0.015f, 0.015f));
	G.Revolve({ { 0.f, 2.5f }, { 4.5f, 2.5f }, { 5.f, -1.f }, { 0.f, -1.f } }, 16, FLinearColor(0.02f, 0.02f, 0.02f));
	Box(G, -9.5f, 0.f, 0.f, 8.f, 0.8f, 1.8f, Chrome);
	Box(G, 9.5f, 0.f, 0.f, 8.f, 0.8f, 1.8f, Chrome);
	Box(G, 0.f, 0.f, -9.5f, 1.8f, 0.8f, 8.f, Chrome);
	Box(G, 0.f, 0.f, 17.5f, 1.2f, 2.f, 1.4f, FLinearColor(0.8f, 0.05f, 0.02f)); // top-centre marker
	G.Commit(Mesh, 0, false);
}

void DriftCarMesh::BuildHandbrakeLever(UProceduralMeshComponent* Mesh)
{
	if (!Mesh) return;
	Mesh->ClearAllMeshSections();
	FProcGeo G;
	Box(G, 0.f, 0.f, 1.5f, 5.f, 3.f, 1.5f, Charcoal);                         // mount
	Box(G, 0.f, 0.f, 15.f, 1.1f, 1.1f, 13.f, Chrome);                         // shaft
	Box(G, 0.f, 0.f, 33.f, 1.9f, 1.9f, 6.f, FLinearColor(0.015f, 0.015f, 0.015f)); // grip
	Box(G, 0.f, 0.f, 39.5f, 2.1f, 2.1f, 0.6f, FLinearColor(0.7f, 0.02f, 0.02f));   // red cap
	G.Commit(Mesh, 0, false);
}

void DriftCarMesh::BuildShifter(UProceduralMeshComponent* Mesh)
{
	if (!Mesh) return;
	Mesh->ClearAllMeshSections();
	FProcGeo G;
	Box(G, 0.f, 0.f, 1.f, 5.5f, 5.5f, 1.5f, FLinearColor(0.012f, 0.012f, 0.012f));
	Box(G, 0.f, 0.f, 9.f, 0.9f, 0.9f, 9.f, Chrome);
	Box(G, 0.f, 0.f, 20.f, 2.6f, 2.6f, 2.6f, FLinearColor(0.015f, 0.015f, 0.015f));
	Box(G, 0.f, 0.f, 22.7f, 1.6f, 1.6f, 0.2f, FLinearColor(0.9f, 0.9f, 0.9f));
	G.Commit(Mesh, 0, false);
}

void DriftCarMesh::BuildHand(UProceduralMeshComponent* Mesh)
{
	if (!Mesh) return;
	Mesh->ClearAllMeshSections();
	FProcGeo G;
	Box(G, 0.f, 0.f, 0.f, 4.f, 3.f, 4.f, Glove);
	Box(G, -4.8f, 0.f, 0.f, 1.2f, 3.6f, 4.2f, FLinearColor(0.5f, 0.03f, 0.02f));
	G.Commit(Mesh, 0, false);
}

void DriftCarMesh::BuildArm(UProceduralMeshComponent* Mesh)
{
	if (!Mesh) return;
	Mesh->ClearAllMeshSections();
	// Octagonal sleeve, thicker at the shoulder end (-X) than at the wrist (+X).
	TArray<TArray<FVector>> Rings;
	for (int32 End = 0; End < 2; ++End)
	{
		const float X = End == 0 ? -0.5f : 0.5f, R = End == 0 ? 4.2f : 3.3f;
		TArray<FVector> Ring;
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = 2.f * PI * (k + 0.5f) / 8.f;
			Ring.Add(FVector(X, FMath::Cos(A) * R, FMath::Sin(A) * R));
		}
		Rings.Add(Ring);
	}
	FProcGeo G;
	G.Loft(Rings, [](int32, int32) { return Sleeve; }, true);
	G.Commit(Mesh, 0, false);
}

FVector DriftCarMesh::LoosePartPivot(int32 Model, int32 Part)
{
	if (Model == 1)
	{
		switch (Part)
		{
		case 0: return FVector(219.f, 0.f, GZ(28.f));
		case 1: return FVector(-221.f, 0.f, GZ(40.f));
		default: return FVector(-215.f, 0.f, GZ(93.f));
		}
	}
	switch (Part)
	{
	case 0: return FVector(210.f, 0.f, GZ(35.f));
	case 1: return FVector(-210.f, 0.f, GZ(44.f));
	default: return FVector(-203.f, 0.f, GZ(92.5f));
	}
}

void DriftCarMesh::BuildLoosePart(UProceduralMeshComponent* Mesh, int32 Model, int32 Part, const FLinearColor& Paint)
{
	if (!Mesh) return;
	Mesh->ClearAllMeshSections();
	FProcGeo G;
	if (Model == 1)
	{
		if (Part == 0)
		{
			Box(G, 220.5f, 0.f, GZ(30.f), 3.f, 66.f, 7.f, Paint);
			Box(G, 216.f, 0.f, GZ(17.5f), 7.f, 74.f, 1.5f, Trim);        // front lip
			Box(G, 222.4f, 0.f, GZ(29.f), 0.3f, 16.f, 5.5f, PlateWhite);
			Box(G, 219.5f, 0.f, GZ(41.f), 2.f, 40.f, 2.5f, Liner);       // intake slot
		}
		else if (Part == 1)
		{
			Box(G, -221.f, 0.f, GZ(40.f), 3.f, 82.f, 8.f, Paint);
			Box(G, -222.6f, 0.f, GZ(42.f), 0.3f, 17.f, 6.5f, PlateWhite);
		}
		else Box(G, -215.f, 0.f, GZ(93.f), 9.f, 82.f, 1.3f, Paint, FRotator(4.f, 0.f, 0.f)); // spoiler lip
	}
	else
	{
		if (Part == 0)
		{
			Box(G, 210.5f, 0.f, GZ(38.f), 3.5f, 74.f, 10.f, Trim);
			Box(G, 208.f, 0.f, GZ(25.5f), 6.f, 72.f, 2.f, Trim);
			Box(G, 212.2f, 0.f, GZ(45.5f), 1.f, 30.f, 1.2f, Liner);
			Box(G, 214.1f, 0.f, GZ(37.f), 0.3f, 16.f, 5.5f, PlateWhite);
		}
		else if (Part == 1)
		{
			Box(G, -210.f, 0.f, GZ(44.f), 4.f, 75.f, 9.f, Trim);
			Box(G, -214.1f, 0.f, GZ(45.f), 0.3f, 17.f, 6.5f, PlateWhite);
		}
		else Box(G, -203.f, 0.f, GZ(92.5f), 5.f, 70.f, 1.2f, Trim, FRotator(6.f, 0.f, 0.f)); // hatch lip spoiler
	}
	const FVector Pivot = LoosePartPivot(Model, Part);
	for (FVector& V : G.Verts) V -= Pivot;
	G.Commit(Mesh, 0, false);
}
