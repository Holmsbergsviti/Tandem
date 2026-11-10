#include "DriftTrackBuilder.h"
#include "Util/ProcGeo.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace TrackParts
{
	const TCHAR* ColorMat = TEXT("/Game/Materials/M_Color.M_Color");
	const TCHAR* VertexMat = TEXT("/Game/Materials/M_VertexColor.M_VertexColor");
	// Vertex colour broken up by fine world-space grain (asphalt, grass): gives the eye texture to read speed from.
	const TCHAR* GroundMatPath = TEXT("/Game/Materials/M_Ground.M_Ground");

	// Road centre line in metres (designed offline: 5.8 km, no section passes within 38 m of another,
	// tightest hairpin ~10 m radius). The road starts at the origin heading +X.
	const FVector2D RoadPoints[] = {
		{0,0},{70,0},{120,-8},{170,8},{220,0},{260.8,-52.7},{241.6,-105.4},{223.7,-154.6},{216.6,-174.6},{223.5,-189.5},
		{236.9,-198.9},{253.2,-200.3},{268,-193.4},{275.4,-173.4},{293.3,-124.2},{312.5,-71.5},{390,-10},{460,30},{540,10},
		{618.1,-38.9},{636.3,-77.8},{652.6,-112.8},{661.7,-132.1},{677.1,-137.7},{693.2,-134.9},{705.8,-124.3},{711.4,-109},
		{702.5,-89.6},{686.1,-54.5},{668,-15.6},{690,0},{760,20},{820,70},{850,140},{912.8,200},{965.6,200},{1014.5,200},
		{1035.8,200.1},{1047.4,211.7},{1051.6,227.5},{1047.4,243.3},{1035.8,254.9},{1014.5,255},{965.6,255},{912.8,255},
		{870,300},{905,345},{885,400},{899,473.1},{938,466.2},{972.8,460.1},{993.8,456.5},{1007.2,465.9},{1014.1,480.8},
		{1012.7,497.1},{1003.3,510.5},{982.3,514.3},{947.5,520.4},{908.5,527.3},{840,580},{800,650},{730,690},{672.8,747.8},
		{685.6,795.6},{697.4,839.6},{702.8,860.2},{694.6,874.4},{680.4,882.6},{664.1,882.6},{649.9,874.4},{644.3,853.8},
		{632.5,809.9},{619.7,762},{560,690},{520,640},{450,660},{420,700},{349.1,761.7},{338.2,823.5},{328,881.7},{324.2,902.7},
		{310.7,912.1},{294.4,913.5},{279.6,906.6},{270.2,893.2},{273.8,872.1},{284.1,813.9},{294.9,752.2},{260,690},{200,720},
		{140,700},{66.5,737.2},{52.9,774.4},{40.8,807.6},{33.4,827.6},{18.6,834.5},{2.3,833.1},{-11.1,823.7},{-18,808.8},
		{-10.8,788.8},{1.2,755.6},{14.8,718.4},{-10,690},{-80,650},{-110,580},{-176.1,520},{-232.2,520},{-284.5,520},
		{-305.8,519.9},{-317.4,508.3},{-321.6,492.5},{-317.4,476.7},{-305.8,465.1},{-284.5,465},{-232.2,465},{-176.1,465},
		{-100,430},{-60,370},{-100,300},{-163.4,224.2},{-206.8,208.4},{-246.4,194},{-266.4,186.6},{-273.3,171.8},{-271.9,155.5},
		{-262.5,142},{-247.6,135.1},{-227.6,142.3},{-188,156.7},{-144.6,172.5},{-110,140},{-90,70},{-50,15}
	};

	// Elevation: a sum of waves that each fit the lap a whole number of times, so the loop closes.
	// {amplitude cm, waves per lap, phase}
	struct FWave { float Amp; float K; float Phase; };
	const FWave HillWaves[] = { {3000, 1, 0.3f}, {1000, 3, 1.7f}, {350, 9, 0.4f}, {120, 23, 2.1f}, {45, 61, 0.9f}, {40, 97, 1.3f} };

	// Practice lot: a flat open square on the left of the start (cm, local). The left barrier opens
	// from GapBefore before the start line to GapAfter after it, and a short slip road joins the two.
	constexpr float PadX0 = -15000.f, PadX1 = 15000.f, PadY0 = -30000.f, PadY1 = -6000.f, PadZ = 0.f;
	constexpr float GapBefore = 400.f, GapAfter = 1600.f;
	const FVector2D PadSpawn(-12000.f, -27000.f);
	const FVector2D PadResets[] = { {-12000.f, -27000.f}, {0.f, -12000.f}, {10000.f, -12000.f} };

	// High-speed loop south of the lot (metres, local, flat at PadZ): a 550 m straight along the lot's
	// south edge, a 110 m-radius 180, a fast 160 m esse and two 90 m corners, about 1.9 km. Wide and
	// barrier-free with flat grass all round, for drifting at around 100 km/h. Points every ~8 m.
	const FVector2D FastPoints[] = {
		{-250.0,-306.0},{-242.0,-306.0},{-234.1,-306.0},{-226.1,-306.0},{-218.1,-306.0},{-210.1,-306.0},{-202.2,-306.0},{-194.2,-306.0},{-186.2,-306.0},{-178.3,-306.0},
		{-170.3,-306.0},{-162.3,-306.0},{-154.3,-306.0},{-146.4,-306.0},{-138.4,-306.0},{-130.4,-306.0},{-122.5,-306.0},{-114.5,-306.0},{-106.5,-306.0},{-98.6,-306.0},
		{-90.6,-306.0},{-82.6,-306.0},{-74.6,-306.0},{-66.7,-306.0},{-58.7,-306.0},{-50.7,-306.0},{-42.8,-306.0},{-34.8,-306.0},{-26.8,-306.0},{-18.8,-306.0},
		{-10.9,-306.0},{-2.9,-306.0},{5.1,-306.0},{13.0,-306.0},{21.0,-306.0},{29.0,-306.0},{37.0,-306.0},{44.9,-306.0},{52.9,-306.0},{60.9,-306.0},
		{68.8,-306.0},{76.8,-306.0},{84.8,-306.0},{92.8,-306.0},{100.7,-306.0},{108.7,-306.0},{116.7,-306.0},{124.6,-306.0},{132.6,-306.0},{140.6,-306.0},
		{148.6,-306.0},{156.5,-306.0},{164.5,-306.0},{172.5,-306.0},{180.4,-306.0},{188.4,-306.0},{196.4,-306.0},{204.3,-306.0},{212.3,-306.0},{220.3,-306.0},
		{228.3,-306.0},{236.2,-306.0},{244.2,-306.0},{252.2,-306.0},{260.1,-306.0},{268.1,-306.0},{276.1,-306.0},{284.1,-306.0},{292.0,-306.0},{300.0,-306.0},
		{308.0,-306.3},{316.0,-307.2},{323.9,-308.6},{331.7,-310.7},{339.3,-313.3},{346.7,-316.4},{353.8,-320.1},{360.7,-324.3},{367.2,-328.9},{373.4,-334.1},
		{379.2,-339.7},{384.6,-345.6},{389.5,-352.0},{393.9,-358.7},{397.8,-365.7},{401.2,-373.0},{404.1,-380.5},{406.4,-388.2},{408.2,-396.0},{409.3,-404.0},
		{409.9,-412.0},{409.9,-420.0},{409.3,-428.0},{408.2,-436.0},{406.4,-443.8},{404.1,-451.5},{401.2,-459.0},{397.8,-466.3},{393.9,-473.3},{389.5,-480.0},
		{384.6,-486.4},{379.2,-492.3},{373.4,-497.9},{367.2,-503.1},{360.7,-507.7},{353.8,-511.9},{346.7,-515.6},{339.3,-518.7},{331.7,-521.3},{323.9,-523.4},
		{316.0,-524.8},{308.0,-525.7},{300.0,-526.0},{292.0,-526.0},{284.0,-526.0},{276.0,-526.0},{268.0,-526.0},{260.0,-526.0},{252.0,-526.0},{244.0,-526.0},
		{236.0,-526.0},{228.0,-526.0},{220.0,-526.0},{212.0,-526.0},{204.0,-526.0},{196.0,-526.0},{188.0,-526.0},{180.0,-526.0},{171.8,-526.2},{163.6,-526.8},
		{155.5,-527.9},{147.4,-529.4},{139.4,-531.2},{131.5,-533.5},{123.7,-536.2},{116.1,-539.3},{108.7,-542.8},{101.4,-546.6},{94.4,-550.8},{87.6,-555.4},
		{81.0,-560.3},{74.7,-565.6},{68.6,-571.1},{62.9,-577.0},{57.4,-583.2},{52.0,-589.3},{46.2,-595.2},{40.2,-600.7},{33.9,-606.0},{27.3,-610.9},
		{20.5,-615.5},{13.4,-619.7},{6.2,-623.5},{-1.3,-627.0},{-8.9,-630.1},{-16.6,-632.8},{-24.5,-635.1},{-32.5,-636.9},{-40.6,-638.4},{-48.7,-639.5},
		{-56.9,-640.1},{-65.1,-640.3},{-73.2,-640.3},{-81.2,-640.3},{-89.2,-640.3},{-97.3,-640.3},{-105.3,-640.3},{-113.4,-640.3},{-121.4,-640.3},{-129.4,-640.3},
		{-137.5,-640.3},{-145.5,-640.3},{-153.5,-640.3},{-161.6,-640.3},{-169.6,-640.3},{-177.7,-640.3},{-185.7,-640.3},{-193.7,-640.3},{-201.8,-640.3},{-209.8,-640.3},
		{-217.8,-640.3},{-225.9,-640.3},{-233.9,-640.3},{-242.0,-640.3},{-250.0,-640.3},{-257.8,-640.0},{-265.6,-638.9},{-273.3,-637.2},{-280.8,-634.9},{-288.0,-631.9},
		{-295.0,-628.3},{-301.6,-624.0},{-307.9,-619.3},{-313.6,-613.9},{-318.9,-608.2},{-323.7,-601.9},{-327.9,-595.3},{-331.6,-588.3},{-334.6,-581.1},{-336.9,-573.6},
		{-338.6,-565.9},{-339.7,-558.2},{-340.0,-550.3},{-340.0,-542.2},{-340.0,-534.1},{-340.0,-525.9},{-340.0,-517.8},{-340.0,-509.7},{-340.0,-501.6},{-340.0,-493.5},
		{-340.0,-485.3},{-340.0,-477.2},{-340.0,-469.1},{-340.0,-461.0},{-340.0,-452.9},{-340.0,-444.7},{-340.0,-436.6},{-340.0,-428.5},{-340.0,-420.4},{-340.0,-412.2},
		{-340.0,-404.1},{-340.0,-396.0},{-339.7,-388.2},{-338.6,-380.4},{-336.9,-372.7},{-334.6,-365.2},{-331.6,-358.0},{-327.9,-351.0},{-323.7,-344.4},{-318.9,-338.1},
		{-313.6,-332.4},{-307.9,-327.1},{-301.6,-322.3},{-295.0,-318.1},{-288.0,-314.4},{-280.8,-311.4},{-273.3,-309.1},{-265.6,-307.4},{-257.8,-306.3}
	};
	constexpr float FastHalfW = 600.f;
	const int32 NumFast = UE_ARRAY_COUNT(FastPoints);

	FVector2D FastP(int32 i) { return FastPoints[((i % NumFast) + NumFast) % NumFast] * 100.0; }
	FVector2D FastDir(int32 i) { return (FastP(i + 1) - FastP(i - 1)).GetSafeNormal(); }
	float FastDist(const FVector2D& P)
	{
		double Best = TNumericLimits<double>::Max();
		for (int32 i = 0; i < NumFast; ++i)
		{
			const FVector2D A = FastP(i), AB = FastP(i + 1) - A;
			const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0), 0.0, 1.0);
			Best = FMath::Min(Best, FVector2D::DistSquared(P, A + AB * T));
		}
		return static_cast<float>(FMath::Sqrt(Best));
	}

	// Turn ladder: an open, flat road east of everything. Corners grow from 5 to 90 deg in 5 deg steps;
	// each angle comes twice, once each way (which way first alternates), so the road keeps heading
	// east. 45 m radius arcs, 50 m straights, a 150 m run-up, turning pads at both ends. ~3.4 km.
	const FVector2D ProgPoints[] = {
		{-450.0,-1000.0},{-442.1,-1000.0},{-434.2,-1000.0},{-426.3,-1000.0},{-418.4,-1000.0},{-410.5,-1000.0},{-402.6,-1000.0},{-394.7,-1000.0},{-386.8,-1000.0},{-378.9,-1000.0},
		{-371.1,-1000.0},{-363.2,-1000.0},{-355.3,-1000.0},{-347.4,-1000.0},{-339.5,-1000.0},{-331.6,-1000.0},{-323.7,-1000.0},{-315.8,-1000.0},{-307.9,-1000.0},{-300.0,-1000.0},
		{-296.1,-999.8},{-287.8,-999.1},{-279.5,-998.4},{-271.2,-997.6},{-262.9,-996.9},{-254.6,-996.2},{-246.3,-995.5},{-242.3,-995.3},{-234.0,-995.3},{-225.7,-995.3},
		{-217.3,-995.3},{-209.0,-995.3},{-200.7,-995.3},{-192.3,-995.3},{-184.5,-996.0},{-176.3,-997.4},{-168.1,-998.9},{-159.9,-1000.3},{-151.7,-1001.8},{-143.5,-1003.2},
		{-135.3,-1004.7},{-127.5,-1005.3},{-119.1,-1005.3},{-110.8,-1005.3},{-102.5,-1005.3},{-94.1,-1005.3},{-85.8,-1005.3},{-77.5,-1005.3},{-65.8,-1003.8},{-57.8,-1001.7},
		{-49.7,-999.5},{-41.7,-997.3},{-33.6,-995.2},{-25.6,-993.0},{-17.5,-990.9},{-5.9,-989.3},{2.4,-989.3},{10.8,-989.3},{19.1,-989.3},{27.4,-989.3},
		{35.8,-989.3},{44.1,-989.3},{51.9,-990.0},{59.5,-992.1},{67.3,-994.9},{75.2,-997.8},{83.0,-1000.6},{90.8,-1003.5},{98.7,-1006.3},{106.5,-1009.2},
		{114.1,-1011.2},{121.9,-1011.9},{130.2,-1011.9},{138.5,-1011.9},{146.9,-1011.9},{155.2,-1011.9},{163.5,-1011.9},{171.9,-1011.9},{181.6,-1010.8},{190.9,-1007.7},
		{198.4,-1004.1},{206.0,-1000.6},{213.6,-997.1},{221.1,-993.6},{228.7,-990.0},{236.2,-986.5},{245.5,-983.4},{255.2,-982.3},{263.6,-982.3},{271.9,-982.3},
		{280.2,-982.3},{288.6,-982.3},{296.9,-982.3},{305.2,-982.3},{313.0,-983.0},{320.6,-985.0},{327.7,-988.3},{334.9,-992.5},{342.2,-996.7},{349.4,-1000.8},
		{356.6,-1005.0},{363.8,-1009.2},{371.0,-1013.3},{378.1,-1016.7},{385.7,-1018.7},{393.5,-1019.4},{401.9,-1019.4},{410.2,-1019.4},{418.5,-1019.4},{426.9,-1019.4},
		{435.2,-1019.4},{443.5,-1019.4},{452.6,-1018.4},{461.4,-1015.7},{469.3,-1011.2},{476.2,-1006.4},{483.0,-1001.7},{489.8,-996.9},{496.6,-992.1},{503.5,-987.3},
		{510.3,-982.5},{518.3,-978.1},{527.0,-975.3},{536.1,-974.4},{544.4,-974.4},{552.8,-974.4},{561.1,-974.4},{569.4,-974.4},{577.8,-974.4},{586.1,-974.4},
		{593.9,-975.1},{601.5,-977.1},{608.6,-980.4},{615.0,-984.9},{621.4,-990.3},{627.8,-995.7},{634.2,-1001.0},{640.6,-1006.4},{647.0,-1011.7},{653.3,-1017.1},
		{659.8,-1021.6},{666.9,-1024.9},{674.4,-1026.9},{682.3,-1027.6},{690.6,-1027.6},{698.9,-1027.6},{707.3,-1027.6},{715.6,-1027.6},{723.9,-1027.6},{732.3,-1027.6},
		{741.0,-1026.7},{749.5,-1024.2},{757.3,-1020.0},{764.1,-1014.4},{770.0,-1008.5},{775.9,-1002.6},{781.8,-996.7},{787.7,-990.9},{793.5,-985.0},{799.4,-979.1},
		{806.3,-973.5},{814.0,-969.3},{822.5,-966.8},{831.3,-965.9},{839.6,-965.9},{847.9,-965.9},{856.3,-965.9},{864.6,-965.9},{872.9,-965.9},{881.3,-965.9},
		{889.1,-966.6},{896.6,-968.6},{903.8,-971.9},{910.2,-976.4},{915.7,-982.0},{921.1,-988.3},{926.4,-994.7},{931.8,-1001.1},{937.2,-1007.5},{942.5,-1013.9},
		{947.9,-1020.3},{953.4,-1025.8},{959.8,-1030.3},{967.0,-1033.6},{974.5,-1035.7},{982.3,-1036.3},{990.7,-1036.3},{999.0,-1036.3},{1007.3,-1036.3},{1015.7,-1036.3},
		{1024.0,-1036.3},{1032.3,-1036.3},{1040.9,-1035.5},{1049.2,-1033.1},{1056.9,-1029.1},{1063.6,-1023.7},{1069.2,-1017.2},{1074.0,-1010.3},{1078.8,-1003.5},{1083.5,-996.7},
		{1088.3,-989.8},{1093.1,-983.0},{1097.9,-976.2},{1103.5,-969.6},{1110.2,-964.3},{1117.9,-960.3},{1126.2,-957.8},{1134.7,-957.0},{1143.1,-957.0},{1151.4,-957.0},
		{1159.7,-957.0},{1168.1,-957.0},{1176.4,-957.0},{1184.7,-957.0},{1192.6,-957.7},{1200.1,-959.7},{1207.2,-963.0},{1213.7,-967.5},{1219.2,-973.1},{1223.7,-979.5},
		{1227.9,-986.7},{1232.0,-993.9},{1236.2,-1001.2},{1240.4,-1008.4},{1244.5,-1015.6},{1248.7,-1022.8},{1253.2,-1029.2},{1258.8,-1034.8},{1265.2,-1039.3},{1272.3,-1042.6},
		{1279.9,-1044.6},{1287.7,-1045.3},{1296.0,-1045.3},{1304.4,-1045.3},{1312.7,-1045.3},{1321.0,-1045.3},{1329.4,-1045.3},{1337.7,-1045.3},{1346.1,-1044.5},{1354.3,-1042.1},
		{1361.9,-1038.3},{1368.6,-1033.0},{1374.2,-1026.7},{1378.5,-1019.3},{1382.0,-1011.8},{1385.5,-1004.2},{1389.0,-996.7},{1392.6,-989.1},{1396.1,-981.6},{1399.6,-974.0},
		{1403.9,-966.7},{1409.5,-960.3},{1416.2,-955.1},{1423.8,-951.2},{1431.9,-948.8},{1440.4,-948.0},{1448.7,-948.0},{1457.1,-948.0},{1465.4,-948.0},{1473.7,-948.0},
		{1482.1,-948.0},{1490.4,-948.0},{1498.2,-948.7},{1505.8,-950.7},{1512.9,-954.1},{1519.3,-958.6},{1524.9,-964.1},{1529.4,-970.5},{1532.7,-977.6},{1535.5,-985.5},
		{1538.4,-993.3},{1541.2,-1001.1},{1544.1,-1009.0},{1546.9,-1016.8},{1549.8,-1024.6},{1553.1,-1031.7},{1557.6,-1038.2},{1563.1,-1043.7},{1569.6,-1048.2},{1576.7,-1051.5},
		{1584.2,-1053.5},{1592.1,-1054.2},{1600.4,-1054.2},{1608.7,-1054.2},{1617.1,-1054.2},{1625.4,-1054.2},{1633.7,-1054.2},{1642.1,-1054.2},{1650.4,-1053.4},{1658.5,-1051.1},
		{1666.0,-1047.3},{1672.7,-1042.2},{1678.3,-1036.0},{1682.6,-1028.8},{1685.5,-1020.9},{1687.7,-1012.8},{1689.8,-1004.8},{1692.0,-996.7},{1694.2,-988.7},{1696.3,-980.6},
		{1698.5,-972.6},{1701.4,-964.7},{1705.7,-957.5},{1711.3,-951.2},{1718.0,-946.1},{1725.5,-942.3},{1733.6,-940.0},{1741.9,-939.2},{1750.3,-939.2},{1758.6,-939.2},
		{1766.9,-939.2},{1775.3,-939.2},{1783.6,-939.2},{1791.9,-939.2},{1799.7,-939.9},{1807.3,-941.9},{1814.4,-945.3},{1820.9,-949.8},{1826.4,-955.3},{1830.9,-961.7},
		{1834.2,-968.8},{1836.2,-976.4},{1837.7,-984.6},{1839.1,-992.8},{1840.6,-1001.0},{1842.0,-1009.2},{1843.5,-1017.4},{1844.9,-1025.7},{1847.0,-1033.2},{1850.3,-1040.3},
		{1854.8,-1046.8},{1860.3,-1052.3},{1866.7,-1056.8},{1873.9,-1060.1},{1881.4,-1062.2},{1889.2,-1062.8},{1897.6,-1062.8},{1905.9,-1062.8},{1914.2,-1062.8},{1922.6,-1062.8},
		{1930.9,-1062.8},{1939.2,-1062.8},{1947.5,-1062.1},{1955.6,-1059.8},{1963.0,-1056.1},{1969.6,-1051.0},{1975.2,-1044.8},{1979.6,-1037.7},{1982.6,-1030.0},{1984.1,-1021.8},
		{1984.8,-1013.5},{1985.5,-1005.2},{1986.3,-996.9},{1987.0,-988.6},{1987.7,-980.3},{1988.4,-972.0},{1989.9,-963.8},{1992.9,-956.0},{1997.3,-948.9},{2002.9,-942.7},
		{2009.5,-937.7},{2017.0,-933.9},{2025.0,-931.6},{2033.3,-930.9},{2041.6,-930.9},{2049.9,-930.9},{2058.3,-930.9},{2066.6,-930.9},{2074.9,-930.9},{2083.3,-930.9},
		{2091.1,-931.6},{2098.7,-933.6},{2105.8,-936.9},{2112.2,-941.4},{2117.7,-946.9},{2122.2,-953.4},{2125.5,-960.5},{2127.6,-968.1},{2128.3,-975.9},{2128.3,-984.2},
		{2128.3,-992.5},{2128.3,-1000.9},{2128.3,-1009.2},{2128.3,-1017.5},{2128.3,-1025.9},{2128.9,-1033.7},{2131.0,-1041.3},{2134.3,-1048.4},{2138.8,-1054.8},{2144.3,-1060.3},
		{2150.8,-1064.8},{2157.9,-1068.2},{2165.4,-1070.2},{2173.3,-1070.9},{2181.3,-1070.9},{2189.3,-1070.9},{2197.3,-1070.9},{2205.3,-1070.9},{2213.3,-1070.9},{2221.3,-1070.9},
		{2229.3,-1070.9},{2237.3,-1070.9},{2245.3,-1070.9},{2253.3,-1070.9},{2261.3,-1070.9},{2269.3,-1070.9},{2277.3,-1070.9},{2285.3,-1070.9},{2293.3,-1070.9}
	};
	struct FProgTurn { int32 Start; int32 Segs; int32 Deg; }; // arc from point Start over Segs segments; + = right
	const FProgTurn ProgTurns[] = { {19, 1, 5}, {26, 1, -5}, {33, 1, -10}, {40, 1, 10}, {47, 1, 15}, {54, 1, -15}, {61, 2, -20}, {69, 2, 20}, {77, 2, 25}, {85, 2, -25}, {93, 3, -30}, {102, 3, 30}, {111, 3, 35}, {120, 3, -35}, {129, 4, -40}, {139, 4, 40}, {149, 4, 45}, {159, 4, -45}, {169, 5, -50}, {180, 5, 50}, {191, 5, 55}, {202, 5, -55}, {213, 6, -60}, {225, 6, 60}, {237, 6, 65}, {249, 6, -65}, {261, 7, -70}, {274, 7, 70}, {287, 7, 75}, {300, 7, -75}, {313, 8, -80}, {327, 8, 80}, {341, 8, 85}, {355, 8, -85}, {369, 9, -90}, {384, 9, 90} };
	constexpr float ProgHalfW = 500.f;
	constexpr float ProgPadR = 3500.f;
	const int32 NumProg = UE_ARRAY_COUNT(ProgPoints);

	FVector2D ProgP(int32 i) { return ProgPoints[FMath::Clamp(i, 0, NumProg - 1)] * 100.0; }
	FVector2D ProgDir(int32 i) { return (ProgP(FMath::Min(i + 1, NumProg - 1)) - ProgP(FMath::Max(i - 1, 0))).GetSafeNormal(); }
	FVector2D ProgPadCentre(bool bEnd)
	{
		return bEnd ? ProgP(NumProg - 1) + ProgDir(NumProg - 1) * 3000.0 : ProgP(0) - ProgDir(0) * 3000.0;
	}
	/** Distance from the ladder's centre line (the end pads count as wide road), cm. */
	float ProgDist(const FVector2D& P)
	{
		static const FBox2D Box = []
		{
			FBox2D B(ForceInit);
			for (int32 i = 0; i < NumProg; ++i) B += ProgP(i);
			return FBox2D(B.Min - FVector2D(20000.0, 20000.0), B.Max + FVector2D(20000.0, 20000.0));
		}();
		if (!Box.IsInside(P)) return 1.e9f;
		double Best = TNumericLimits<double>::Max();
		for (int32 i = 0; i + 1 < NumProg; ++i)
		{
			const FVector2D A = ProgP(i), AB = ProgP(i + 1) - A;
			const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0), 0.0, 1.0);
			Best = FMath::Min(Best, FVector2D::DistSquared(P, A + AB * T));
		}
		float D = static_cast<float>(FMath::Sqrt(Best));
		for (int32 E = 0; E < 2; ++E)
		{
			D = FMath::Min(D, FMath::Max(0.f, static_cast<float>(FVector2D::Distance(P, ProgPadCentre(E == 1))) - (ProgPadR - ProgHalfW)));
		}
		return D;
	}

	float PadDist(const FVector2D& P)
	{
		const double DX = FMath::Max3(PadX0 - P.X, 0.0, P.X - PadX1);
		const double DY = FMath::Max3(PadY0 - P.Y, 0.0, P.Y - PadY1);
		return static_cast<float>(FMath::Sqrt(DX * DX + DY * DY));
	}

	TArray<FVector> Slice(const TArray<FVector>& A, int32 From, int32 To)
	{
		TArray<FVector> Out;
		for (int32 i = From; i <= To; ++i) Out.Add(A[i]);
		return Out;
	}

	/** Smooth value noise in [-1, 1]. */
	float Hash2(int32 X, int32 Y)
	{
		uint32 H = static_cast<uint32>(X) * 374761393u + static_cast<uint32>(Y) * 668265263u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H ^= H >> 16;
		return static_cast<float>(H & 0xFFFFu) / 32767.5f - 1.f;
	}
	float ValueNoise(float X, float Y)
	{
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = X - static_cast<float>(X0), FY = Y - static_cast<float>(Y0);
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY);
		const float A = FMath::Lerp(Hash2(X0, Y0), Hash2(X0 + 1, Y0), SX);
		const float B = FMath::Lerp(Hash2(X0, Y0 + 1), Hash2(X0 + 1, Y0 + 1), SX);
		return FMath::Lerp(A, B, SY);
	}
	float SmoothStep01(float E0, float E1, float X)
	{
		const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	UInstancedStaticMeshComponent* MakeISM(AActor* Owner, const TCHAR* Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Owner->GetRootComponent());
		C->SetStaticMesh(Mesh);
		C->SetMobility(EComponentMobility::Static);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return C;
	}
}

ADriftTrackBuilder::ADriftTrackBuilder()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* RootC = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootC);

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetupAttachment(RootC);
	FillSpline();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeF(TEXT("/Engine/BasicShapes/Cone.Cone"));

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	TerrainMesh->SetupAttachment(RootC);
	TerrainMesh->bUseComplexAsSimpleCollision = true;
	TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));

	RoadMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RoadMesh"));
	RoadMesh->SetupAttachment(RootC);
	RoadMesh->bUseComplexAsSimpleCollision = true; // the car's wheels and body ride on the real surface
	RoadMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RoadMesh->SetCastShadow(false);

	WallMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WallMesh"));
	WallMesh->SetupAttachment(RootC);
	// Solid convex blocks for collision (added in Build): a thin two-faced trimesh let a fast car
	// slip through between physics steps.
	WallMesh->bUseComplexAsSimpleCollision = false;
	WallMesh->SetCollisionProfileName(TEXT("BlockAll"));

	TreeTrunks = TrackParts::MakeISM(this, TEXT("TreeTrunks"), CylF.Object);
	TreeCrowns = TrackParts::MakeISM(this, TEXT("TreeCrowns"), ConeF.Object);
	TreeTrunks->SetCullDistances(0, 40000);
	TreeCrowns->SetCullDistances(0, 45000);
	Posts = TrackParts::MakeISM(this, TEXT("Posts"), CylF.Object);
	PostTops = TrackParts::MakeISM(this, TEXT("PostTops"), CylF.Object);
	Cones = TrackParts::MakeISM(this, TEXT("Cones"), ConeF.Object);
	Posts->SetCullDistances(0, 30000);
	PostTops->SetCullDistances(0, 30000);
	Cones->SetCullDistances(0, 25000);
	Scenery.Create(this, TEXT("Pass"));
}

void ADriftTrackBuilder::ApplyQuality(int32 Level)
{
	Scenery.ApplyQuality(Level);
	const int32 PostCull = FMath::RoundToInt(DriftSceneryUtil::CullFor(Level, 30000.f));
	Posts->SetCullDistances(0, PostCull);
	PostTops->SetCullDistances(0, PostCull);
	Cones->SetCullDistances(0, FMath::RoundToInt(DriftSceneryUtil::CullFor(Level, 25000.f)));
	Cones->SetCastShadow(Level >= 2);
	Posts->SetCastShadow(Level >= 2);
	PostTops->SetCastShadow(Level >= 2);
}

void ADriftTrackBuilder::FillSpline()
{
	if (!Spline) return;
	Spline->ClearSplinePoints(false);
	for (const FVector2D& P : TrackParts::RoadPoints)
	{
		Spline->AddSplinePoint(FVector(P.X * 100.0, P.Y * 100.0, 0.0), ESplineCoordinateSpace::Local, false);
	}
	Spline->SetClosedLoop(true, false);
	Spline->UpdateSpline();
}

void ADriftTrackBuilder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void ADriftTrackBuilder::BeginPlay()
{
	Super::BeginPlay();
	// Rebuild at runtime too: packaged builds don't run construction scripts and runtime materials aren't saved.
	Build();
}

void ADriftTrackBuilder::Tint(UPrimitiveComponent* Comp, const FLinearColor& Color)
{
	if (!Comp) return;
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TrackParts::ColorMat))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		Comp->SetMaterial(0, MID);
	}
}

float ADriftTrackBuilder::RoadHeightAt(float Distance) const
{
	const float Len = Spline ? Spline->GetSplineLength() : 1.f;
	const float T = 2.f * PI * Distance / FMath::Max(Len, 1.f);
	float Z = 0.f;
	for (const TrackParts::FWave& W : TrackParts::HillWaves)
	{
		Z += W.Amp * (FMath::Sin(W.K * T + W.Phase) - FMath::Sin(W.Phase)); // 0 at the start
	}
	// Small bumps in the asphalt, rougher in some stretches than others.
	const float Rough = 0.5f + 0.5f * FMath::Sin(7.f * T + 0.8f);
	const float Bumps = (FMath::Sin(600.f * T + 0.2f) + 0.6f * FMath::Sin(1100.f * T + 2.6f)) * BumpHeight * 0.625f * Rough;
	return Z * HillScale + Bumps;
}

void ADriftTrackBuilder::Build()
{
	if (!Spline || !RoadMesh || !WallMesh || !TerrainMesh) return;
	FillSpline(); // always the designed road, whatever got saved in the map

	const float Len = Spline->GetSplineLength();
	const int32 N = FMath::Max(16, FMath::CeilToInt(Len / SegmentLength));
	const float HalfW = RoadWidth * 0.5f;
	const float WallIn = HalfW + RunOff;
	const FVector Z = FVector::UpVector;

	// ---- Road centre line with its height, and the flat sideways direction ----
	TArray<FVector> Mid, Side;
	for (int32 i = 0; i <= N; ++i)
	{
		const float D = Len * (i % N) / N;
		FVector P = Spline->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::Local);
		P.Z = RoadHeightAt(D);
		Mid.Add(P);
		FVector R = Spline->GetRightVectorAtDistanceAlongSpline(D, ESplineCoordinateSpace::Local);
		R.Z = 0.f;
		Side.Add(R.GetSafeNormal());
	}
	auto Edge = [&](float Offset, float Height)
	{
		TArray<FVector> Out;
		Out.Reserve(N + 1);
		for (int32 i = 0; i <= N; ++i) Out.Add(Mid[i] + Side[i] * Offset + Z * Height);
		return Out;
	};

	// Road samples where the left barrier is open onto the practice lot: [GapLo, N) and [0, GapHi].
	const float SegLen = Len / N;
	const int32 GapLo = FMath::Clamp(N - FMath::CeilToInt(TrackParts::GapBefore / SegLen), N / 2, N - 1);
	const int32 GapHi = FMath::Clamp(FMath::CeilToInt(TrackParts::GapAfter / SegLen), 1, N / 2);

	// ---- Section 0: asphalt (with collision). Right edge first so the faces point up. ----
	FProcGeo Asphalt;
	Asphalt.Ribbon(Edge(HalfW, 3.f), Edge(-HalfW, 3.f), [&](int32 i)
	{
		const float V = 0.03f + 0.006f * FMath::Sin(i * 0.37f) + 0.004f * FMath::Sin(i * 1.91f);
		return FLinearColor(V, V, V * 1.08f);
	});
	Asphalt.Commit(RoadMesh, 0, true);

	// ---- Section 1: grass/gravel shoulders up to the barriers (with collision) ----
	FProcGeo Shoulder;
	auto ShoulderColor = [](int32 i) { const float V = 0.8f + 0.2f * FMath::Sin(i * 0.53f); return FLinearColor(0.07f * V, 0.1f * V, 0.035f * V); };
	Shoulder.Ribbon(Edge(WallIn, 0.f), Edge(HalfW, 2.5f), ShoulderColor);
	Shoulder.Ribbon(TrackParts::Slice(Edge(-HalfW, 2.5f), GapHi, GapLo), TrackParts::Slice(Edge(-WallIn, 0.f), GapHi, GapLo), ShoulderColor);
	Shoulder.Commit(RoadMesh, 1, true);

	// ---- Section 2: white edge lines and a dashed yellow centre line (no collision) ----
	FProcGeo Lines;
	const FLinearColor LineC(0.9f, 0.9f, 0.88f), Yellow(0.85f, 0.6f, 0.05f);
	Lines.Ribbon(Edge(-HalfW + 45.f, 3.6f), Edge(-HalfW + 30.f, 3.6f), [&](int32) { return LineC; });
	Lines.Ribbon(Edge(HalfW - 30.f, 3.6f), Edge(HalfW - 45.f, 3.6f), [&](int32) { return LineC; });
	{
		const TArray<FVector> CL = Edge(8.f, 3.6f), CR = Edge(-8.f, 3.6f);
		for (int32 i = 0; i < N; ++i)
		{
			if (i % 4 >= 2) continue; // 5 m dash, 5 m gap
			Lines.Quad(Lines.AddVert(CL[i], Yellow), Lines.AddVert(CL[i + 1], Yellow), Lines.AddVert(CR[i + 1], Yellow), Lines.AddVert(CR[i], Yellow));
		}
	}
	// Road wear: dark repair patches and paler worn stretches, so the asphalt isn't one uniform strip.
	{
		FRandomStream PatchRng(77);
		for (int32 k = 0; k < N / 12; ++k)
		{
			const int32 I0 = PatchRng.RandRange(0, N - 4);
			const int32 Count = PatchRng.RandRange(1, 3);
			const float Lo = PatchRng.FRandRange(-HalfW + 60.f, HalfW - 260.f);
			const float Hi = FMath::Min(Lo + PatchRng.FRandRange(150.f, 380.f), HalfW - 50.f);
			const FLinearColor PatchC = PatchRng.FRand() < 0.65f ? FLinearColor(0.016f, 0.016f, 0.018f) : FLinearColor(0.055f, 0.054f, 0.052f);
			Lines.Ribbon(TrackParts::Slice(Edge(Hi, 3.3f), I0, I0 + Count), TrackParts::Slice(Edge(Lo, 3.3f), I0, I0 + Count), [&](int32) { return PatchC; });
		}
	}
	Lines.Commit(RoadMesh, 2, false);

	// ---- Section 3: the practice lot, dead flat, and the slip road to it (with collision) ----
	FProcGeo Pad;
	{
		const float Top = TrackParts::PadZ + 3.f;
		constexpr float PadCell = 1000.f;
		const int32 PX = FMath::CeilToInt((TrackParts::PadX1 - TrackParts::PadX0) / PadCell);
		const int32 PY = FMath::CeilToInt((TrackParts::PadY1 - TrackParts::PadY0) / PadCell);
		auto PadRow = [&](int32 Y)
		{
			TArray<FVector> Row;
			for (int32 X = 0; X <= PX; ++X)
				Row.Add(FVector(FMath::Lerp(TrackParts::PadX0, TrackParts::PadX1, static_cast<float>(X) / PX),
					FMath::Lerp(TrackParts::PadY0, TrackParts::PadY1, static_cast<float>(Y) / PY), Top));
			return Row;
		};
		for (int32 Y = 0; Y < PY; ++Y)
		{
			// Row Y + 1 has the larger Y, which is the right-hand side going +X: faces point up.
			Pad.Ribbon(PadRow(Y + 1), PadRow(Y), [&](int32 X)
			{
				const float V = 0.045f * (1.f + 0.12f * TrackParts::ValueNoise(X * 0.7f, Y * 0.7f));
				return FLinearColor(V, V, V * 1.06f);
			});
		}
	}
	// Slip road: from the left edge of the asphalt in the gap, widening down to the lot's top edge.
	TArray<int32> GapIdx;
	for (int32 i = GapLo; i < N; ++i) GapIdx.Add(i);
	for (int32 i = 0; i <= GapHi; ++i) GapIdx.Add(i);
	const int32 NC = GapIdx.Num();
	constexpr int32 ConnRows = 8;
	TArray<TArray<FVector>> ConnRow;
	{
		const FVector RoadFirst = Mid[GapIdx[0]] - Side[GapIdx[0]] * HalfW;
		const FVector RoadLast = Mid[GapIdx[NC - 1]] - Side[GapIdx[NC - 1]] * HalfW;
		const float CX = 0.5f * static_cast<float>(RoadFirst.X + RoadLast.X);
		for (int32 r = 0; r <= ConnRows; ++r)
		{
			const float T = static_cast<float>(r) / ConnRows;
			const float TZ = T * T * (3.f - 2.f * T);
			TArray<FVector> Row;
			for (int32 c = 0; c < NC; ++c)
			{
				const int32 i = GapIdx[c];
				const FVector RoadP = Mid[i] - Side[i] * HalfW + Z * 3.f;
				const FVector PadP(CX + FMath::Lerp(-2500.f, 2500.f, static_cast<float>(c) / (NC - 1)), TrackParts::PadY1, TrackParts::PadZ + 3.f);
				FVector P = FMath::Lerp(RoadP, PadP, T);
				P.Z = FMath::Lerp(RoadP.Z, PadP.Z, TZ);
				Row.Add(P);
			}
			ConnRow.Add(Row);
		}
	}
	auto ConnColor = [](int32 i) { const float V = 0.04f + 0.004f * FMath::Sin(i * 0.9f); return FLinearColor(V, V, V * 1.07f); };
	for (int32 r = 0; r < ConnRows; ++r) Pad.Ribbon(ConnRow[r], ConnRow[r + 1], ConnColor); // road side is on the right going +X
	{
		// Sloped skirts down the sides of the slip road into the ground.
		TArray<FVector> WIn, WOut, EIn, EOut;
		for (int32 r = 0; r <= ConnRows; ++r)
		{
			WIn.Add(ConnRow[r][0]);
			WOut.Add(ConnRow[r][0] + FVector(-600.f, 0.f, -90.f));
			EIn.Add(ConnRow[r][NC - 1]);
			EOut.Add(ConnRow[r][NC - 1] + FVector(600.f, 0.f, -90.f));
		}
		const FLinearColor Dirt(0.06f, 0.07f, 0.035f);
		Pad.Ribbon(WIn, WOut, [&](int32) { return Dirt; });  // heading towards the lot, the slip road is on the right
		Pad.Ribbon(EOut, EIn, [&](int32) { return Dirt; });
	}
	// High-speed loop asphalt, level with the lot (its north edge is the lot's south edge).
	auto FastEdge = [&](float Offset, float Height)
	{
		TArray<FVector> Out;
		for (int32 i = 0; i <= TrackParts::NumFast; ++i)
		{
			const FVector2D D = TrackParts::FastDir(i);
			Out.Add(FVector(TrackParts::FastP(i) + FVector2D(-D.Y, D.X) * Offset, TrackParts::PadZ + Height));
		}
		return Out;
	};
	Pad.Ribbon(FastEdge(TrackParts::FastHalfW, 3.f), FastEdge(-TrackParts::FastHalfW, 3.f), [](int32 i)
	{
		const float V = 0.035f + 0.005f * FMath::Sin(i * 0.41f);
		return FLinearColor(V, V, V * 1.07f);
	});
	// Turn ladder asphalt and its two round turning pads (pads a hair lower so the road wins where they overlap).
	auto ProgEdge = [&](float Offset, float Height)
	{
		TArray<FVector> Out;
		for (int32 i = 0; i < TrackParts::NumProg; ++i)
		{
			const FVector2D D = TrackParts::ProgDir(i);
			Out.Add(FVector(TrackParts::ProgP(i) + FVector2D(-D.Y, D.X) * Offset, TrackParts::PadZ + Height));
		}
		return Out;
	};
	Pad.Ribbon(ProgEdge(TrackParts::ProgHalfW, 3.f), ProgEdge(-TrackParts::ProgHalfW, 3.f), [](int32 i)
	{
		const float V = 0.036f + 0.005f * FMath::Sin(i * 0.37f);
		return FLinearColor(V, V, V * 1.07f);
	});
	for (int32 E = 0; E < 2; ++E)
	{
		const FVector2D PC = TrackParts::ProgPadCentre(E == 1);
		const FLinearColor PadCol(0.042f, 0.042f, 0.045f);
		const int32 CI = Pad.AddVert(FVector(PC, TrackParts::PadZ + 2.5f), PadCol);
		const int32 Ring0 = Pad.Verts.Num();
		constexpr int32 Segs = 48;
		for (int32 k = 0; k < Segs; ++k)
		{
			const float Ang = 2.f * PI * k / Segs;
			Pad.AddVert(FVector(PC + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * TrackParts::ProgPadR, TrackParts::PadZ + 2.5f), PadCol);
		}
		for (int32 k = 0; k < Segs; ++k) Pad.Tri(CI, Ring0 + (k + 1) % Segs, Ring0 + k); // faces up
	}
	Pad.Commit(RoadMesh, 3, true);
	// Ground under and beside the slip road must stay below it.
	FBox2D ConnBox(ForceInit);
	float ConnLowZ = TrackParts::PadZ;
	for (const TArray<FVector>& Row : ConnRow)
	{
		for (const FVector& P : Row) ConnBox += FVector2D(P.X, P.Y);
	}
	for (const FVector& P : ConnRow[0]) ConnLowZ = FMath::Min(ConnLowZ, static_cast<float>(P.Z) - 3.f);
	ConnBox = FBox2D(ConnBox.Min - FVector2D(800.f, 0.f), ConnBox.Max + FVector2D(800.f, 0.f));

	// ---- Section 4: painted practice marks on the lot (no collision) ----
	FProcGeo Marks;
	{
		const FLinearColor Paint(0.85f, 0.85f, 0.82f);
		const float MZ = TrackParts::PadZ + 3.6f;
		auto Circle = [&](const FVector2D& C, float Rad)
		{
			TArray<FVector> In, Out;
			const int32 Seg = FMath::Max(32, FMath::CeilToInt(Rad / 120.f));
			for (int32 k = 0; k <= Seg; ++k)
			{
				const float A = 2.f * PI * k / Seg;
				const FVector2D U(FMath::Cos(A), FMath::Sin(A));
				In.Add(FVector(C + U * (Rad - 15.f), MZ));
				Out.Add(FVector(C + U * (Rad + 15.f), MZ));
			}
			Marks.Ribbon(In, Out, [&](int32) { return Paint; }); // counter-clockwise: inside is on the right
		};
		Circle(FVector2D(-10500.f, -10500.f), 1200.f);              // donut circle
		Circle(FVector2D(-2000.f, -18000.f), 2000.f);               // figure eight
		Circle(FVector2D(2000.f, -18000.f), 2000.f);
		Circle(FVector2D(9500.f, -20000.f), 3500.f);                // big sweeper
		// Border line 3 m in from the edge.
		const float In0 = 300.f - 15.f, In1 = 300.f + 15.f;
		auto Rect = [&](float Inset)
		{
			return TArray<FVector>{ FVector(TrackParts::PadX0 + Inset, TrackParts::PadY0 + Inset, MZ), FVector(TrackParts::PadX1 - Inset, TrackParts::PadY0 + Inset, MZ),
				FVector(TrackParts::PadX1 - Inset, TrackParts::PadY1 - Inset, MZ), FVector(TrackParts::PadX0 + Inset, TrackParts::PadY1 - Inset, MZ),
				FVector(TrackParts::PadX0 + Inset, TrackParts::PadY0 + Inset, MZ) };
		};
		Marks.Ribbon(Rect(In1), Rect(In0), [&](int32) { return Paint; });
		// Turn ladder: edge lines, dashed centre line, kerbs through every corner.
		{
			const float HW = TrackParts::ProgHalfW;
			Marks.Ribbon(ProgEdge(HW - 30.f, 3.6f), ProgEdge(HW - 45.f, 3.6f), [&](int32) { return Paint; });
			Marks.Ribbon(ProgEdge(-HW + 45.f, 3.6f), ProgEdge(-HW + 30.f, 3.6f), [&](int32) { return Paint; });
			const TArray<FVector> CLa = ProgEdge(10.f, 3.6f), CLb = ProgEdge(-10.f, 3.6f);
			for (int32 i = 0; i + 1 < TrackParts::NumProg; i += 2)
				Marks.Quad(Marks.AddVert(CLa[i], Paint), Marks.AddVert(CLa[i + 1], Paint), Marks.AddVert(CLb[i + 1], Paint), Marks.AddVert(CLb[i], Paint));
			const FLinearColor Red(0.6f, 0.03f, 0.02f);
			const TArray<FVector> PRO = ProgEdge(HW, 3.5f), PRI = ProgEdge(HW - 100.f, 3.5f), PLI = ProgEdge(-HW + 100.f, 3.5f), PLO = ProgEdge(-HW, 3.5f);
			for (const TrackParts::FProgTurn& Turn : TrackParts::ProgTurns)
			{
				const int32 I0 = FMath::Max(0, Turn.Start - 1), I1 = FMath::Min(TrackParts::NumProg - 2, Turn.Start + Turn.Segs);
				for (int32 i = I0; i <= I1; ++i)
				{
					for (int32 k = 0; k < 4; ++k)
					{
						const FLinearColor KC = ((i * 4 + k) % 2 == 0) ? Red : Paint;
						const float A0 = k / 4.f, A1 = (k + 1) / 4.f;
						Marks.Quad(Marks.AddVert(FMath::Lerp(PRO[i], PRO[i + 1], A0), KC), Marks.AddVert(FMath::Lerp(PRO[i], PRO[i + 1], A1), KC),
							Marks.AddVert(FMath::Lerp(PRI[i], PRI[i + 1], A1), KC), Marks.AddVert(FMath::Lerp(PRI[i], PRI[i + 1], A0), KC));
						Marks.Quad(Marks.AddVert(FMath::Lerp(PLI[i], PLI[i + 1], A0), KC), Marks.AddVert(FMath::Lerp(PLI[i], PLI[i + 1], A1), KC),
							Marks.AddVert(FMath::Lerp(PLO[i], PLO[i + 1], A1), KC), Marks.AddVert(FMath::Lerp(PLO[i], PLO[i + 1], A0), KC));
					}
				}
			}
		}
		// Dashed centre line on the high-speed loop (8 m dash, 8 m gap).
		{
			const TArray<FVector> CLa = FastEdge(10.f, 3.6f), CLb = FastEdge(-10.f, 3.6f);
			for (int32 i = 0; i < TrackParts::NumFast; i += 2)
				Marks.Quad(Marks.AddVert(CLa[i], Paint), Marks.AddVert(CLa[i + 1], Paint), Marks.AddVert(CLb[i + 1], Paint), Marks.AddVert(CLb[i], Paint));
		}

		// High-speed loop: white edge lines, and red/white kerbs painted on the edges through the bends.
		Marks.Ribbon(FastEdge(TrackParts::FastHalfW - 30.f, 3.6f), FastEdge(TrackParts::FastHalfW - 45.f, 3.6f), [&](int32) { return Paint; });
		Marks.Ribbon(FastEdge(-TrackParts::FastHalfW + 45.f, 3.6f), FastEdge(-TrackParts::FastHalfW + 30.f, 3.6f), [&](int32) { return Paint; });
		const FLinearColor KerbRed(0.6f, 0.03f, 0.02f);
		const TArray<FVector> RO = FastEdge(TrackParts::FastHalfW, 3.5f), RI = FastEdge(TrackParts::FastHalfW - 100.f, 3.5f);
		const TArray<FVector> LI = FastEdge(-TrackParts::FastHalfW + 100.f, 3.5f), LO = FastEdge(-TrackParts::FastHalfW, 3.5f);
		for (int32 i = 0; i < TrackParts::NumFast; ++i)
		{
			const double Turn = FMath::Abs(FVector2D::CrossProduct(TrackParts::FastDir(i), TrackParts::FastDir(i + 1)));
			const double SegLenM = (TrackParts::FastP(i + 1) - TrackParts::FastP(i)).Size() / 100.0;
			if (Turn / FMath::Max(SegLenM, 0.1) < 1.0 / 250.0) continue; // only bends tighter than 250 m
			for (int32 k = 0; k < 4; ++k)
			{
				const FLinearColor KC = ((i * 4 + k) % 2 == 0) ? KerbRed : Paint;
				const float A0 = k / 4.f, A1 = (k + 1) / 4.f;
				Marks.Quad(Marks.AddVert(FMath::Lerp(RO[i], RO[i + 1], A0), KC), Marks.AddVert(FMath::Lerp(RO[i], RO[i + 1], A1), KC),
					Marks.AddVert(FMath::Lerp(RI[i], RI[i + 1], A1), KC), Marks.AddVert(FMath::Lerp(RI[i], RI[i + 1], A0), KC));
				Marks.Quad(Marks.AddVert(FMath::Lerp(LI[i], LI[i + 1], A0), KC), Marks.AddVert(FMath::Lerp(LI[i], LI[i + 1], A1), KC),
					Marks.AddVert(FMath::Lerp(LO[i], LO[i + 1], A1), KC), Marks.AddVert(FMath::Lerp(LO[i], LO[i + 1], A0), KC));
			}
		}
	}
	Marks.Commit(RoadMesh, 4, false);

	// ---- Barriers: concrete with red/white painted tops; they reach down past the terrain edge ----
	FProcGeo Walls;
	const FLinearColor Concrete(0.42f, 0.42f, 0.4f), Red(0.6f, 0.02f, 0.015f), White(0.85f, 0.85f, 0.85f);
	for (int32 SideSign = -1; SideSign <= 1; SideSign += 2)
	{
		const float O0 = SideSign * WallIn, O1 = SideSign * (WallIn + 40.f);
		const int32 From = SideSign < 0 ? GapHi : 0, To = SideSign < 0 ? GapLo : N; // left side stops at the gap
		Walls.Ribbon(TrackParts::Slice(Edge(O0, -150.f), From, To), TrackParts::Slice(Edge(O0, WallHeight), From, To), [&](int32) { return Concrete; });
		Walls.Ribbon(TrackParts::Slice(Edge(O1, -150.f), From, To), TrackParts::Slice(Edge(O1, WallHeight), From, To), [&](int32) { return Concrete * 0.8f; });
		const TArray<FVector> T0 = Edge(O0, WallHeight), T1 = Edge(O1, WallHeight);
		for (int32 i = From; i < To; ++i)
		{
			const FLinearColor C = ((i / 2) % 2 == 0) ? Red : White;
			Walls.Quad(Walls.AddVert(T0[i], C), Walls.AddVert(T0[i + 1], C), Walls.AddVert(T1[i + 1], C), Walls.AddVert(T1[i], C));
		}
	}
	Walls.Commit(WallMesh, 0, false);

	// Collision: solid convex blocks, thicker and taller than the visible barrier. A block covers several
	// segments on straights and gets shorter in tight bends so it follows the curve.
	// All blocks go in with one call: AddCollisionConvexMesh re-cooks every block on each call,
	// which with ~1800 blocks froze the game on a black screen at startup.
	TArray<TArray<FVector>> WallBlocks;
	for (int32 SideSign = -1; SideSign <= 1; SideSign += 2)
	{
		const float O0 = SideSign * WallIn, O1 = SideSign * (WallIn + 160.f);
		const TArray<FVector> B0 = Edge(O0, -60.f), B1 = Edge(O1, -60.f), H0 = Edge(O0, WallHeight + 80.f), H1 = Edge(O1, WallHeight + 80.f);
		const int32 From = SideSign < 0 ? GapHi : 0, To = SideSign < 0 ? GapLo : N;
		int32 i = From;
		while (i < To)
		{
			int32 j = i + 1;
			while (j < To && j - i < 8 && FVector::DotProduct(Side[i], Side[j + 1]) > 0.996f) ++j; // < ~5 degrees of bend
			const FVector Ext = (Mid[j] - Mid[i]).GetSafeNormal() * 15.f; // overlap neighbours: no gaps at joints
			WallBlocks.Add({ B0[i] - Ext, B0[j] + Ext, B1[i] - Ext, B1[j] + Ext,
				H0[i] - Ext, H0[j] + Ext, H1[i] - Ext, H1[j] + Ext });
			i = j;
		}
	}
	WallMesh->SetCollisionConvexMeshes(WallBlocks);

	// ---- Terrain: hills and valleys that meet the road just below the barrier bases ----
	FBox2D RoadBox(ForceInit);
	for (const FVector& P : Mid) RoadBox += FVector2D(P.X, P.Y);
	for (int32 i = 0; i < TrackParts::NumFast; ++i) RoadBox += TrackParts::FastP(i);
	for (int32 i = 0; i < TrackParts::NumProg; ++i) RoadBox += TrackParts::ProgP(i);
	for (int32 E = 0; E < 2; ++E)
	{
		RoadBox += TrackParts::ProgPadCentre(E == 1) - FVector2D(TrackParts::ProgPadR, TrackParts::ProgPadR);
		RoadBox += TrackParts::ProgPadCentre(E == 1) + FVector2D(TrackParts::ProgPadR, TrackParts::ProgPadR);
	}
	const FVector2D Min = RoadBox.Min - FVector2D(TerrainMargin, TerrainMargin);
	const FVector2D Max = RoadBox.Max + FVector2D(TerrainMargin, TerrainMargin);
	const float Cell = FMath::Max(TerrainCell, 300.f);
	const int32 NX = FMath::CeilToInt((Max.X - Min.X) / Cell) + 1;
	const int32 NY = FMath::CeilToInt((Max.Y - Min.Y) / Cell) + 1;

	// Coarse road samples (~10 m apart) for the nearest-road search and the far-field height.
	constexpr int32 Coarse = 4;
	TArray<int32> CoarseIdx;
	for (int32 i = 0; i < N; i += Coarse) CoarseIdx.Add(i);
	const float FlatZone = WallIn + 1400.f;   // terrain sits just under the road out to here
	const float BlendZone = FlatZone + 6000.f; // then rises/falls into the hills by here

	// Distance from a point to the road (2D) and the road height there.
	auto NearestRoad = [&](const FVector2D& P, float& OutDist, float& OutZ)
	{
		int32 Best = 0;
		double BestD2 = TNumericLimits<double>::Max();
		for (int32 c : CoarseIdx)
		{
			const double D2 = FVector2D::DistSquared(P, FVector2D(Mid[c].X, Mid[c].Y));
			if (D2 < BestD2) { BestD2 = D2; Best = c; }
		}
		OutDist = TNumericLimits<float>::Max();
		OutZ = Mid[Best].Z;
		for (int32 k = Best - Coarse - 1; k <= Best + Coarse; ++k)
		{
			const int32 A = ((k % N) + N) % N;
			const FVector2D PA(Mid[A].X, Mid[A].Y), PB(Mid[A + 1].X, Mid[A + 1].Y);
			const FVector2D AB = PB - PA;
			const float T = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(P - PA, AB) / FMath::Max(AB.SizeSquared(), 1.0)), 0.f, 1.f);
			const float D = static_cast<float>(FVector2D::Distance(P, PA + AB * T));
			if (D < OutDist) { OutDist = D; OutZ = FMath::Lerp(static_cast<float>(Mid[A].Z), static_cast<float>(Mid[A + 1].Z), T); }
		}
	};

	TArray<float> Heights, Blends;
	Heights.SetNumUninitialized(NX * NY);
	Blends.SetNumUninitialized(NX * NY);
	for (int32 y = 0; y < NY; ++y)
	{
		for (int32 x = 0; x < NX; ++x)
		{
			const FVector2D P(Min.X + x * Cell, Min.Y + y * Cell);
			float RoadDist, RoadZ;
			NearestRoad(P, RoadDist, RoadZ);
			const float Blend = TrackParts::SmoothStep01(FlatZone, BlendZone, RoadDist);
			float FarZ = RoadZ;
			if (Blend > 0.f)
			{
				// Far from the road, follow a smooth average of the road's height around it.
				double WSum = 0.0, ZSum = 0.0;
				for (int32 c : CoarseIdx)
				{
					const double D2 = FVector2D::DistSquared(P, FVector2D(Mid[c].X, Mid[c].Y));
					const double Wt = 1.0 / FMath::Square(D2 + 2.5e7);
					WSum += Wt;
					ZSum += Wt * Mid[c].Z;
				}
				FarZ = static_cast<float>(ZSum / WSum);
			}
			const float Hills = HillHeight * (0.65f * TrackParts::ValueNoise(P.X / 45000.f, P.Y / 45000.f)
				+ 0.25f * TrackParts::ValueNoise(P.X / 14000.f + 17.f, P.Y / 14000.f)
				+ 0.1f * TrackParts::ValueNoise(P.X / 9000.f, P.Y / 9000.f + 31.f));
			// Hills rise more away from the road than they fall, so it reads as a mountain pass.
			const float Shaped = Hills > 0.f ? Hills * 1.4f : Hills * 0.7f;
			float Hgt = FMath::Lerp(RoadZ, FarZ, Blend) - 60.f + Blend * Shaped;
			// Around the practice lot the ground is flat and level with it (the road keeps priority near it).
			const float PadInf = FMath::Max(1.f - TrackParts::SmoothStep01(2000.f, 8000.f, TrackParts::PadDist(P)),
				FMath::Max(1.f - TrackParts::SmoothStep01(TrackParts::FastHalfW + 3000.f, TrackParts::FastHalfW + 9000.f, TrackParts::FastDist(P)),
					1.f - TrackParts::SmoothStep01(TrackParts::ProgHalfW + 3000.f, TrackParts::ProgHalfW + 9000.f, TrackParts::ProgDist(P))));
			const float PadW = PadInf * TrackParts::SmoothStep01(WallIn + 100.f, FlatZone, RoadDist);
			Hgt = FMath::Lerp(Hgt, TrackParts::PadZ - 5.f, PadW);
			if (ConnBox.IsInside(P)) Hgt = FMath::Min(Hgt, ConnLowZ - 60.f);
			Heights[y * NX + x] = Hgt;
			Blends[y * NX + x] = Blend * (1.f - PadInf);
		}
	}

	FProcGeo Terrain;
	for (int32 y = 0; y < NY; ++y)
	{
		for (int32 x = 0; x < NX; ++x)
		{
			const int32 I = y * NX + x;
			const float HX0 = Heights[y * NX + FMath::Max(x - 1, 0)], HX1 = Heights[y * NX + FMath::Min(x + 1, NX - 1)];
			const float HY0 = Heights[FMath::Max(y - 1, 0) * NX + x], HY1 = Heights[FMath::Min(y + 1, NY - 1) * NX + x];
			const FVector Normal = FVector(-(HX1 - HX0) / (2.f * Cell), -(HY1 - HY0) / (2.f * Cell), 1.f).GetSafeNormal();
			// Grass, darker and browner by the road, grey rock on steep faces.
			const float Var = 0.85f + 0.3f * TrackParts::ValueNoise(x * 0.37f, y * 0.37f);
			const FLinearColor Grass(0.05f * Var, 0.12f * Var, 0.03f * Var);
			const FLinearColor Verge(0.07f, 0.09f, 0.04f);
			const FLinearColor Rock(0.16f, 0.15f, 0.13f);
			FLinearColor C = FMath::Lerp(Verge, Grass, Blends[I]);
			C = FMath::Lerp(C, Rock, TrackParts::SmoothStep01(0.82f, 0.65f, static_cast<float>(Normal.Z)));
			Terrain.AddVert(FVector(Min.X + x * Cell, Min.Y + y * Cell, Heights[I]), C);
			Terrain.Normals.Add(Normal);
		}
	}
	for (int32 y = 0; y + 1 < NY; ++y)
	{
		for (int32 x = 0; x + 1 < NX; ++x)
		{
			const int32 A = y * NX + x, B = A + 1, C = A + NX + 1, D = A + NX;
			Terrain.Tri(A, D, C); // faces point up
			Terrain.Tri(A, C, B);
		}
	}
	Terrain.Commit(TerrainMesh, 0, true);

	// ---- Materials ----
	if (UMaterialInterface* VMat = LoadObject<UMaterialInterface>(nullptr, TrackParts::VertexMat))
	{
		auto MakeMID = [&](float Rough)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(VMat, this);
			MID->SetScalarParameterValue(TEXT("Roughness"), Rough);
			MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			MID->SetScalarParameterValue(TEXT("Emissive"), 0.f);
			return MID;
		};
		UMaterialInterface* GroundBase = LoadObject<UMaterialInterface>(nullptr, TrackParts::GroundMatPath);
		auto MakeGround = [&](float Rough)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(GroundBase ? GroundBase : VMat, this);
			MID->SetScalarParameterValue(TEXT("Roughness"), Rough);
			MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			MID->SetScalarParameterValue(TEXT("Emissive"), 0.f);
			return MID;
		};
		RoadMesh->SetMaterial(0, MakeGround(0.75f));
		RoadMesh->SetMaterial(1, MakeGround(0.95f));
		RoadMesh->SetMaterial(2, MakeMID(0.5f));
		RoadMesh->SetMaterial(3, MakeGround(0.75f));
		RoadMesh->SetMaterial(4, MakeMID(0.5f));
		WallMesh->SetMaterial(0, MakeMID(0.85f));
		TerrainMesh->SetMaterial(0, MakeGround(0.95f));
	}
	Tint(TreeTrunks, FLinearColor(0.12f, 0.07f, 0.03f));
	Tint(TreeCrowns, FLinearColor(0.025f, 0.09f, 0.025f));

	// ---- Trees, bushes and rocks on the hillsides, kept clear of the road ----
	// The forest changes across the map: pine stands, leafy groves, birch and a few autumn patches.
	TreeTrunks->ClearInstances();
	TreeCrowns->ClearInstances();
	Scenery.Paint(this);
	Scenery.Clear();
	FRandomStream Rng(1337);
	// Ground height and steepness at a grid position (cells), or false if too near the road / off the grid.
	auto Ground = [&](float GX, float GY, float MinBlend, FVector& OutP, float& OutRise)
	{
		if (GX < 0.f || GY < 0.f || GX > static_cast<float>(NX - 2) || GY > static_cast<float>(NY - 2)) return false;
		const int32 X0 = FMath::FloorToInt(GX), Y0 = FMath::FloorToInt(GY);
		const float FX = GX - X0, FY = GY - Y0;
		const int32 I = Y0 * NX + X0;
		if (Blends[I] < MinBlend || Blends[I + NX + 1] < MinBlend) return false;
		const float H00 = Heights[I], H10 = Heights[I + 1], H01 = Heights[I + NX], H11 = Heights[I + NX + 1];
		OutRise = FMath::Max(FMath::Max(H00, H10), FMath::Max(H01, H11)) - FMath::Min(FMath::Min(H00, H10), FMath::Min(H01, H11));
		const float H = FMath::Lerp(FMath::Lerp(H00, H10, FX), FMath::Lerp(H01, H11, FX), FY);
		OutP = FVector(Min.X + GX * Cell, Min.Y + GY * Cell, H - 20.f);
		return true;
	};
	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < TreeCount * 6 && Placed < TreeCount; ++Attempt)
	{
		const float GX = Rng.FRandRange(0.f, static_cast<float>(NX - 2)), GY = Rng.FRandRange(0.f, static_cast<float>(NY - 2));
		FVector P;
		float Rise = 0.f;
		if (!Ground(GX, GY, 0.15f, P, Rise) || Rise > Cell * 0.8f) continue; // near the road, or a cliff
		const float Conifer = FMath::Clamp(0.5f + 0.7f * TrackParts::ValueNoise(P.X / 60000.f + 3.f, P.Y / 60000.f), 0.05f, 0.95f);
		const float Autumn = 0.8f * TrackParts::SmoothStep01(0.25f, 0.7f, TrackParts::ValueNoise(P.X / 30000.f, P.Y / 30000.f + 9.f));
		Scenery.AddTree(Rng, P, Conifer, Autumn);
		// Undergrowth next to some trees.
		if (Rng.FRand() < 0.3f)
		{
			FVector B;
			if (Ground(GX + Rng.FRandRange(-0.4f, 0.4f), GY + Rng.FRandRange(-0.4f, 0.4f), 0.1f, B, Rise)) Scenery.AddBush(Rng, B);
		}
		++Placed;
	}
	// Rocks: boulders on the steep faces, smaller stones scattered in the grass.
	for (int32 Attempt = 0, Rocks = 0; Attempt < 6000 && Rocks < 700; ++Attempt)
	{
		FVector P;
		float Rise = 0.f;
		if (!Ground(Rng.FRandRange(0.f, static_cast<float>(NX - 2)), Rng.FRandRange(0.f, static_cast<float>(NY - 2)), 0.08f, P, Rise)) continue;
		const bool bSteep = Rise > Cell * 0.35f;
		if (!bSteep && Rng.FRand() < 0.6f) continue;
		Scenery.AddRock(Rng, P, bSteep ? Rng.FRandRange(200.f, 650.f) : Rng.FRandRange(60.f, 220.f));
		++Rocks;
	}

	// ---- Speed references: marker posts along the high-speed loop, cones round and on the practice lot ----
	Tint(Posts, FLinearColor(0.8f, 0.8f, 0.78f));
	Tint(PostTops, FLinearColor(0.7f, 0.04f, 0.02f));
	Tint(Cones, FLinearColor(0.9f, 0.25f, 0.02f));
	Posts->ClearInstances();
	PostTops->ClearInstances();
	Cones->ClearInstances();
	for (int32 i = 0; i < TrackParts::NumFast; i += 2) // every ~16 m, 2.5 m off each edge
	{
		const FVector2D Dir = TrackParts::FastDir(i);
		const FVector2D Rt(-Dir.Y, Dir.X);
		for (int32 SideSign = -1; SideSign <= 1; SideSign += 2)
		{
			const FVector2D P2 = TrackParts::FastP(i) + Rt * (SideSign * (TrackParts::FastHalfW + 250.f));
			if (TrackParts::PadDist(P2) < 200.f) continue; // not on the lot
			const FVector Foot(P2, TrackParts::PadZ);
			Posts->AddInstance(FTransform(FRotator::ZeroRotator, Foot + FVector(0.f, 0.f, 50.f), FVector(0.12f, 0.12f, 1.f)));
			PostTops->AddInstance(FTransform(FRotator::ZeroRotator, Foot + FVector(0.f, 0.f, 90.f), FVector(0.13f, 0.13f, 0.12f)));
		}
	}
	for (int32 i = 0; i < TrackParts::NumProg; i += 2) // turn ladder marker posts
	{
		const FVector2D Dir = TrackParts::ProgDir(i);
		const FVector2D Rt(-Dir.Y, Dir.X);
		for (int32 SideSign = -1; SideSign <= 1; SideSign += 2)
		{
			const FVector2D P2 = TrackParts::ProgP(i) + Rt * (SideSign * (TrackParts::ProgHalfW + 250.f));
			if (FVector2D::Distance(P2, TrackParts::ProgPadCentre(false)) < TrackParts::ProgPadR + 200.f
				|| FVector2D::Distance(P2, TrackParts::ProgPadCentre(true)) < TrackParts::ProgPadR + 200.f) continue;
			const FVector Foot(P2, TrackParts::PadZ);
			Posts->AddInstance(FTransform(FRotator::ZeroRotator, Foot + FVector(0.f, 0.f, 50.f), FVector(0.12f, 0.12f, 1.f)));
			PostTops->AddInstance(FTransform(FRotator::ZeroRotator, Foot + FVector(0.f, 0.f, 90.f), FVector(0.13f, 0.13f, 0.12f)));
		}
	}
	auto AddCone = [&](const FVector2D& P2)
	{
		Cones->AddInstance(FTransform(FRotator::ZeroRotator, FVector(P2, TrackParts::PadZ + 3.f + 35.f), FVector(0.35f, 0.35f, 0.7f)));
	};
	for (float CY = TrackParts::PadY0 + 1500.f; CY <= TrackParts::PadY1 - 500.f; CY += 1500.f)
	{
		AddCone(FVector2D(TrackParts::PadX0 + 150.f, CY));
		AddCone(FVector2D(TrackParts::PadX1 - 150.f, CY));
	}
	for (float CX = TrackParts::PadX0 + 1500.f; CX <= TrackParts::PadX1 - 1500.f; CX += 1500.f)
	{
		if (CX < ConnBox.Min.X - 500.f || CX > ConnBox.Max.X + 500.f) AddCone(FVector2D(CX, TrackParts::PadY1 - 150.f));
	}
	for (int32 k = 0; k < 6; ++k) AddCone(FVector2D(3000.f + k * 2000.f, -9000.f)); // slalom
}

FTransform ADriftTrackBuilder::GetAreaStart(int32 Index, FString& OutName) const
{
	if (Index == 1)
	{
		OutName = TEXT("HIGH-SPEED LOOP");
		const FVector2D Dir = TrackParts::FastDir(0);
		return FTransform(FRotator(0.f, GetActorRotation().Yaw + FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f),
			GetActorTransform().TransformPosition(FVector(TrackParts::FastP(0), TrackParts::PadZ + 83.f)));
	}
	if (Index == 3)
	{
		OutName = TEXT("TURN LADDER   5 to 90 deg");
		const FVector2D Dir = TrackParts::ProgDir(0);
		return FTransform(FRotator(0.f, GetActorRotation().Yaw + FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f),
			GetActorTransform().TransformPosition(FVector(TrackParts::ProgP(0), TrackParts::PadZ + 83.f)));
	}
	if (Index == 2)
	{
		OutName = TEXT("MOUNTAIN PASS");
		const float Dist = 3000.f; // just past the slip road
		const FTransform T = Spline->GetTransformAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
		return FTransform(FRotator(0.f, T.Rotator().Yaw, 0.f), T.GetLocation() + FVector(0, 0, RoadHeightAt(Dist) + 80.f));
	}
	OutName = TEXT("PRACTICE LOT");
	return GetStartTransform();
}

bool ADriftTrackBuilder::GetLadderHint(const FVector& WorldPos, const FVector& WorldFwd, FString& OutText) const
{
	const FVector L = GetActorTransform().InverseTransformPosition(WorldPos);
	const FVector2D P(L.X, L.Y);
	int32 Near = INDEX_NONE;
	double NearD2 = FMath::Square(3000.0); // only within 30 m of the road
	for (int32 i = 0; i < TrackParts::NumProg; ++i)
	{
		const double D2 = FVector2D::DistSquared(P, TrackParts::ProgP(i));
		if (D2 < NearD2) { NearD2 = D2; Near = i; }
	}
	if (Near == INDEX_NONE) return false;
	const FVector LF = GetActorTransform().InverseTransformVector(WorldFwd);
	const bool bForward = FVector2D::DotProduct(FVector2D(LF.X, LF.Y), TrackParts::ProgDir(Near)) >= 0.0;
	auto PathM = [](int32 A, int32 B)
	{
		double Sum = 0.0;
		for (int32 i = FMath::Min(A, B); i < FMath::Max(A, B); ++i) Sum += FVector2D::Distance(TrackParts::ProgP(i), TrackParts::ProgP(i + 1));
		return static_cast<float>(Sum / 100.0);
	};
	const int32 NT = UE_ARRAY_COUNT(TrackParts::ProgTurns);
	for (int32 k = 0; k < NT; ++k)
	{
		const TrackParts::FProgTurn& Turn = TrackParts::ProgTurns[bForward ? k : NT - 1 - k];
		const int32 Enter = bForward ? Turn.Start : Turn.Start + Turn.Segs; // where the arc begins, driving this way
		const int32 Leave = bForward ? Turn.Start + Turn.Segs : Turn.Start;
		if (bForward ? Leave < Near : Leave > Near) continue; // already through it
		const int32 Deg = bForward ? Turn.Deg : -Turn.Deg;
		const float Dist = (bForward ? Enter > Near : Enter < Near) ? PathM(Near, Enter) : 0.f;
		const FString When = Dist > 4.f ? FString::Printf(TEXT("in %.0f m"), Dist) : FString(TEXT("NOW"));
		OutText = FString::Printf(TEXT("NEXT  %s %d\u00B0   %s"), Deg > 0 ? TEXT("RIGHT") : TEXT("LEFT"), FMath::Abs(Deg), *When);
		return true;
	}
	OutText = TEXT("END OF THE LADDER: turn round on the pad");
	return true;
}

FTransform ADriftTrackBuilder::GetStartTransform() const
{
	// Start on the practice lot, facing its long open side; the slip road to the pass is ahead on the right.
	const FVector P = GetActorTransform().TransformPosition(FVector(TrackParts::PadSpawn, TrackParts::PadZ + 83.f));
	return FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), P);
}

TArray<FTransform> ADriftTrackBuilder::GetCheckpoints() const
{
	TArray<FTransform> Out;
	const float Len = Spline->GetSplineLength();
	for (float D = 0.f; D < Len - CheckpointSpacing * 0.5f; D += CheckpointSpacing)
	{
		const FTransform T = Spline->GetTransformAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
		Out.Add(FTransform(FRotator(0.f, T.Rotator().Yaw, 0.f), T.GetLocation() + FVector(0, 0, RoadHeightAt(D))));
	}
	// Reset points along the high-speed loop, every ~100 m, facing its direction.
	for (int32 i = 0; i < TrackParts::NumFast; i += 12)
	{
		const FVector2D D = TrackParts::FastDir(i);
		Out.Add(FTransform(FRotator(0.f, GetActorRotation().Yaw + FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.f),
			GetActorTransform().TransformPosition(FVector(TrackParts::FastP(i), TrackParts::PadZ + 3.f))));
	}
	// Reset points along the turn ladder, every ~100 m, facing along it.
	for (int32 i = 0; i < TrackParts::NumProg; i += 12)
	{
		const FVector2D D = TrackParts::ProgDir(i);
		Out.Add(FTransform(FRotator(0.f, GetActorRotation().Yaw + FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.f),
			GetActorTransform().TransformPosition(FVector(TrackParts::ProgP(i), TrackParts::PadZ + 3.f))));
	}
	// Reset points on the practice lot too, so Backspace there keeps you there.
	for (const FVector2D& R : TrackParts::PadResets)
	{
		Out.Add(FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), GetActorTransform().TransformPosition(FVector(R, TrackParts::PadZ + 3.f))));
	}
	return Out;
}
