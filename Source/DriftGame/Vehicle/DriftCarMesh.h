#pragma once

#include "CoreMinimal.h"

class UProceduralMeshComponent;

/**
 * Procedural models of the cars. Model 0: an AE86-style 1980s hatchback (white over black two-tone,
 * pop-up headlight lids, full-width tail lamps, 8-spoke 14" wheels, right-hand drive). Model 1: an
 * NSX-style mid-engine coupe (low wedge nose with pop-up lamps, cab-forward canopy with a black roof, side
 * intakes, long rear deck with a full-width tail lamp bar). Both share the cabin and door layout.
 * Body-local cm, +X forward, +Y right, road at Z = DriftCarMesh::GroundZ when at rest.
 */
namespace DriftCarMesh
{
	/** Body-local height of the road surface with the car sitting on its springs. */
	constexpr float GroundZ = -53.f;

	/** Door hinge line (front edge of each door). Side 0 = left (-Y), 1 = right (+Y, driver side). */
	constexpr float DoorHingeX = 53.f;
	constexpr float DoorHingeY = 80.5f;
	constexpr float DoorHingeYNSX = 87.5f;
	constexpr float DoorHingeZ = GroundZ + 55.f;
	inline FVector DoorHinge(int32 Side, int32 Model = 0)
	{
		const float Y = Model == 1 ? DoorHingeYNSX : DoorHingeY;
		return FVector(DoorHingeX, Side == 0 ? -Y : Y, DoorHingeZ);
	}

	/** Body: section 0 paint/trim, 1 lights (emissive), 2 window glass (translucent).
	 *  Doors (optional, built relative to DoorHinge): section 0 paint/trim, 1 glass. */
	void BuildBody(UProceduralMeshComponent* Mesh, const FLinearColor& Paint,
		UProceduralMeshComponent* DoorLeft = nullptr, UProceduralMeshComponent* DoorRight = nullptr, int32 Model = 0);
	/** Section 0: tyre. Section 1: rim. Wheel axis is Y, outer face at +Y. */
	void BuildWheel(UProceduralMeshComponent* Mesh, float Radius, float HalfWidth);

	// Cockpit parts (body-local cm, vertex coloured). Right-hand drive.
	/** Section 0: trim, seats, harnesses, mirror. Section 1: lit instrument dials. Section 2: roll cage. */
	/** MirrorDrop lowers the rear-view mirror (cm) for a car with a lower roof. */
	void BuildInterior(UProceduralMeshComponent* Mesh, float MirrorDrop = 0.f);
	/** Instrument needle: points +Z from its pivot in the Y-Z plane (turn it with roll). */
	void BuildNeedle(UProceduralMeshComponent* Mesh, float Length);
	/** Dial centres (body-local cm) of the rev counter (0-9000 rpm) and speedometer (0-220 km/h). */
	inline FVector TachCentre() { return FVector(27.f, 38.f - 6.8f, GroundZ + 81.f); }
	inline FVector SpeedoCentre() { return FVector(27.f, 38.f + 6.8f, GroundZ + 81.f); }
	/** The needles sweep this many degrees clockwise, starting at lower left. */
	constexpr float GaugeSweepDeg = 270.f;
	/** Rim in the mesh X-Z plane, axis Y. */
	void BuildSteeringWheel(UProceduralMeshComponent* Mesh);
	/** Vertical drift (hydraulic) handbrake: stick points up +Z from its pivot, handle at Z = 30. */
	void BuildHandbrakeLever(UProceduralMeshComponent* Mesh);
	/** Stick pointing up +Z from its pivot, knob at Z = 20. */
	void BuildShifter(UProceduralMeshComponent* Mesh);
	void BuildHand(UProceduralMeshComponent* Mesh);
	/** 1 cm long along X (scale X = length), tapered sleeve about 8 cm thick. */
	void BuildArm(UProceduralMeshComponent* Mesh);
}
