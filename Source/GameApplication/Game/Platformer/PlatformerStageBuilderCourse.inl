	static void BuildCourse(EntityCommandBuffer& commands) {
		// 1. Meadow tutorial: wide ground, a single slow enemy, and an early save.
		QueueBlock(commands, "Meadow_Start",
			Vector3(0.0f, -0.5f, 10.0f), Vector3(12.0f, 1.0f, 24.0f), Grass());
		QueueBlock(commands, "Meadow_LeftBank",
			Vector3(-8.0f, -1.0f, 12.0f), Vector3(4.0f, 1.0f, 24.0f), Orchard());
		QueueBlock(commands, "Meadow_RightBank",
			Vector3(8.0f, -1.0f, 12.0f), Vector3(4.0f, 1.0f, 24.0f), Orchard());
		QueueEnemy(commands, "Enemy_Meadow",
			Vector3(0.0f, 0.45f, 16.0f), Vector3(1.0f, 0.0f, 0.0f), 2.2f, 1.15f);
		QueueCheckpoint(commands, "Checkpoint_Meadow",
			Vector3(0.0f, 0.6f, 20.0f));

		// 2. Garden turn: a readable uphill ramp, X-axis turn, and lateral mover.
		QueueBlock(commands, "Ramp_Garden",
			Vector3(3.0f, 0.65f, 27.0f), Vector3(5.0f, 0.8f, 10.0f), Stone(), -15.0f);
		QueueBlock(commands, "Garden_Plateau",
			Vector3(4.0f, 1.6f, 35.0f), Vector3(8.0f, 1.0f, 8.0f), Stone());
		QueueBlock(commands, "Garden_Bridge",
			Vector3(0.0f, 2.0f, 41.0f), Vector3(12.0f, 0.7f, 6.0f), Sand());
		QueueBlock(commands, "Garden_LeftIsland",
			Vector3(-6.0f, 2.4f, 47.0f), Vector3(8.0f, 0.8f, 8.0f), Grass());
		QueueBlock(commands, "Garden_SafetyFloor",
			Vector3(0.0f, -2.5f, 48.0f), Vector3(24.0f, 1.0f, 42.0f), Grass());
		QueueMovingPlatform(commands, "Garden_Crossing",
			Vector3(-3.0f, 3.0f, 54.0f), Vector3(9.0f, 0.0f, 0.0f), 4.6f, 0.1f);
		QueueBlock(commands, "Garden_RightPlateau",
			Vector3(6.0f, 3.0f, 59.0f), Vector3(8.0f, 1.0f, 8.0f), Orchard());
		QueueEnemy(commands, "Enemy_Garden",
			Vector3(4.0f, 2.55f, 35.0f), Vector3(1.0f, 0.0f, 0.0f), 2.0f, 1.25f);
		QueueCheckpoint(commands, "Checkpoint_Garden",
			Vector3(6.0f, 4.1f, 59.0f));
		QueueCameraZone(commands, "CameraZone_GardenTurn",
			Vector3(0.0f, 3.0f, 43.0f), Vector3(22.0f, 8.0f, 22.0f),
			PlatformerCameraController::Profile::TripleJump, true);

		// 3. Sky orchard: Z progression bends through X while Y rises gradually.
		QueueBlock(commands, "Orchard_Ramp",
			Vector3(4.0f, 4.2f, 65.0f), Vector3(6.0f, 0.8f, 10.0f), Orchard(), -10.0f);
		QueueBlock(commands, "Orchard_Island_A",
			Vector3(2.0f, 5.0f, 72.0f), Vector3(8.0f, 1.0f, 7.0f), Orchard());
		QueueBlock(commands, "Orchard_Island_B",
			Vector3(-5.0f, 6.0f, 79.0f), Vector3(7.0f, 1.0f, 7.0f), Grass());
		QueueBlock(commands, "Orchard_SafetyFloor",
			Vector3(0.0f, 1.5f, 82.0f), Vector3(28.0f, 1.0f, 50.0f), Grass());
		QueueBlock(commands, "Orchard_RescueStep",
			Vector3(4.0f, 2.4f, 61.0f), Vector3(6.0f, 0.8f, 4.0f), Orchard());
		QueueMovingPlatform(commands, "Orchard_Lift",
			Vector3(0.0f, 6.6f, 85.0f), Vector3(0.0f, 3.0f, 0.0f), 4.2f, 0.35f);
		QueueBlock(commands, "Orchard_HighIsland",
			Vector3(5.0f, 9.0f, 92.0f), Vector3(8.0f, 1.0f, 8.0f), Orchard());
		QueueBlock(commands, "Orchard_DescentIsland",
			Vector3(0.0f, 7.2f, 99.0f), Vector3(9.0f, 1.0f, 8.0f), Stone());
		QueueEnemy(commands, "Enemy_Orchard",
			Vector3(-5.0f, 6.95f, 79.0f), Vector3(0.0f, 0.0f, 1.0f), 1.8f, 1.15f);
		QueueCheckpoint(commands, "Checkpoint_Orchard",
			Vector3(5.0f, 10.1f, 92.0f));
		QueueCameraZone(commands, "CameraZone_Orchard",
			Vector3(0.0f, 7.0f, 82.0f), Vector3(28.0f, 18.0f, 44.0f),
			PlatformerCameraController::Profile::WallKick, true);

		// 4. Ruins: broader combat space, metallic materials, and a calm boss gate.
		QueueBlock(commands, "Ruins_Courtyard",
			Vector3(-5.0f, 5.5f, 107.0f), Vector3(10.0f, 1.0f, 10.0f), Ruin());
		QueueBlock(commands, "Ruins_Bridge",
			Vector3(1.0f, 5.0f, 114.0f), Vector3(12.0f, 0.8f, 5.0f), Ruin());
		QueueBlock(commands, "BossGate_Rest",
			Vector3(0.0f, 4.0f, 121.0f), Vector3(12.0f, 1.0f, 8.0f), Guide());
		QueueBlock(commands, "BossGate_Step1",
			Vector3(0.0f, 3.0f, 126.0f), Vector3(12.0f, 1.0f, 4.0f), Ruin());
		QueueBlock(commands, "BossGate_Step2",
			Vector3(0.0f, 2.0f, 130.0f), Vector3(12.0f, 1.0f, 4.0f), Ruin());
		QueueBlock(commands, "BossGate_Step3",
			Vector3(0.0f, 1.0f, 134.0f), Vector3(12.0f, 1.0f, 4.0f), Ruin());
		QueueEnemy(commands, "Enemy_Ruins",
			Vector3(-5.0f, 6.45f, 107.0f), Vector3(1.0f, 0.0f, 0.0f), 2.3f, 1.3f);
		QueueCheckpoint(commands, "Checkpoint_BossGate",
			Vector3(0.0f, 5.1f, 121.0f));
		QueueCameraZone(commands, "CameraZone_Ruins",
			Vector3(0.0f, 6.0f, 109.0f), Vector3(26.0f, 14.0f, 28.0f),
			PlatformerCameraController::Profile::Course, false);

		// 5. Boss arena: large, bounded, uncluttered, and readable for beginners.
		QueueBlock(commands, "BossArena_Floor",
			Vector3(0.0f, -0.5f, 147.0f), Vector3(24.0f, 1.0f, 30.0f), Arena());
		QueueBlock(commands, "BossArena_LeftWall",
			Vector3(-12.5f, 1.5f, 147.0f), Vector3(1.0f, 3.0f, 30.0f), Ruin());
		QueueBlock(commands, "BossArena_RightWall",
			Vector3(12.5f, 1.5f, 147.0f), Vector3(1.0f, 3.0f, 30.0f), Ruin());
		QueueBlock(commands, "BossArena_BackWall",
			Vector3(0.0f, 1.5f, 162.0f), Vector3(24.0f, 3.0f, 1.0f), Ruin());
		QueueBlock(commands, "Arena_Pillar_1",
			Vector3(-10.0f, 2.0f, 137.0f), Vector3(1.5f, 4.0f, 1.5f), Guide());
		QueueBlock(commands, "Arena_Pillar_2",
			Vector3(10.0f, 2.0f, 137.0f), Vector3(1.5f, 4.0f, 1.5f), Guide());
		QueueBlock(commands, "Arena_Pillar_3",
			Vector3(-10.0f, 2.0f, 157.0f), Vector3(1.5f, 4.0f, 1.5f), Guide());
		QueueBlock(commands, "Arena_Pillar_4",
			Vector3(10.0f, 2.0f, 157.0f), Vector3(1.5f, 4.0f, 1.5f), Guide());
		QueueBoss(commands, Vector3(0.0f, 1.0f, 148.0f));

		BuildCoins(commands);
		BuildScenery(commands);
	}

	static void BuildCoins(EntityCommandBuffer& commands) {
		static const std::array<Vector3, 30> CoinPositions = {
			Vector3(0.0f, 1.0f, 4.0f),
			Vector3(0.0f, 1.0f, 8.0f),
			Vector3(-2.0f, 1.0f, 12.0f),
			Vector3(2.0f, 1.0f, 16.0f),
			Vector3(0.0f, 1.2f, 20.0f),
			Vector3(1.5f, 1.6f, 24.0f),
			Vector3(3.0f, 2.4f, 28.0f),
			Vector3(4.0f, 3.0f, 34.0f),
			Vector3(1.0f, 3.0f, 39.0f),
			Vector3(-2.0f, 3.0f, 43.0f),
			Vector3(-5.0f, 3.8f, 47.0f),
			Vector3(-2.0f, 4.0f, 52.0f),
			Vector3(2.0f, 4.0f, 55.0f),
			Vector3(5.0f, 4.5f, 59.0f),
			Vector3(4.0f, 5.2f, 64.0f),
			Vector3(2.0f, 6.3f, 70.0f),
			Vector3(-1.0f, 6.5f, 74.0f),
			Vector3(-5.0f, 7.3f, 78.0f),
			Vector3(-2.0f, 7.6f, 82.0f),
			Vector3(0.0f, 8.0f, 85.0f),
			Vector3(2.0f, 9.5f, 88.0f),
			Vector3(5.0f, 10.4f, 92.0f),
			Vector3(2.0f, 9.0f, 96.0f),
			Vector3(0.0f, 8.6f, 99.0f),
			Vector3(-4.0f, 6.8f, 105.0f),
			Vector3(-6.0f, 6.8f, 109.0f),
			Vector3(-1.0f, 6.0f, 113.0f),
			Vector3(2.0f, 6.0f, 116.0f),
			Vector3(0.0f, 5.6f, 120.0f),
			Vector3(0.0f, 4.6f, 128.0f)
		};
		for(size_t i = 0; i < CoinPositions.size(); ++i) {
			const std::string name =
				"Coin_" + (i + 1 < 10 ? std::string("0") : std::string()) +
				std::to_string(i + 1);
			QueueCoin(commands, name, CoinPositions[i]);
		}
	}

	static void BuildScenery(EntityCommandBuffer& commands) {
		static const std::array<Vector3, 16> TreePositions = {
			Vector3(-6.0f, 2.1f, 5.0f),
			Vector3(6.0f, 2.1f, 7.0f),
			Vector3(-7.0f, 2.1f, 17.0f),
			Vector3(7.0f, 2.1f, 20.0f),
			Vector3(-10.0f, 2.4f, 35.0f),
			Vector3(10.0f, 3.4f, 39.0f),
			Vector3(-10.0f, 4.0f, 54.0f),
			Vector3(11.0f, 4.4f, 61.0f),
			Vector3(-9.0f, 6.0f, 68.0f),
			Vector3(10.0f, 7.0f, 73.0f),
			Vector3(-10.0f, 8.2f, 84.0f),
			Vector3(10.0f, 10.0f, 92.0f),
			Vector3(-11.0f, 7.0f, 104.0f),
			Vector3(10.0f, 6.5f, 116.0f),
			Vector3(-14.0f, 2.0f, 141.0f),
			Vector3(14.0f, 2.0f, 153.0f)
		};
		for(size_t i = 0; i < TreePositions.size(); ++i) {
			const std::string name =
				"Tree_" + (i + 1 < 10 ? std::string("0") : std::string()) +
				std::to_string(i + 1);
			QueueTree(commands, name, TreePositions[i],
				5.0f + static_cast<float>(i % 3) * 0.35f);
		}

		QueueProp(commands, BearDecorationPrefab, "Ruin_BearStatue_Left",
			Vector3(-9.0f, 6.2f, 110.0f), Vector3(0.34f, 0.34f, 0.34f),
			Ruin(), 35.0f);
		QueueProp(commands, BearDecorationPrefab, "Ruin_BearStatue_Right",
			Vector3(8.0f, 5.8f, 118.0f), Vector3(0.30f, 0.30f, 0.30f),
			Ruin(), -35.0f);

		QueueBlock(commands, "Ruin_Column_1",
			Vector3(-7.0f, 7.1f, 103.0f), Vector3(1.2f, 3.2f, 1.2f), Ruin());
		QueueBlock(commands, "Ruin_Column_2",
			Vector3(7.0f, 7.5f, 108.0f), Vector3(1.2f, 4.0f, 1.2f), Ruin());
		QueueBlock(commands, "Ruin_Column_3",
			Vector3(-7.0f, 7.3f, 116.0f), Vector3(1.2f, 3.6f, 1.2f), Ruin());
		QueueBlock(commands, "Ruin_Column_4",
			Vector3(7.0f, 7.6f, 121.0f), Vector3(1.2f, 4.2f, 1.2f), Ruin());

		QueueProp(commands, GlowOrbPrefab, "GuideLamp_1",
			Vector3(-3.0f, 1.2f, 20.0f), Vector3(0.28f, 0.28f, 0.28f),
			WarmGlow());
		QueueProp(commands, GlowOrbPrefab, "GuideLamp_2",
			Vector3(7.0f, 4.7f, 59.0f), Vector3(0.28f, 0.28f, 0.28f),
			WarmGlow());
		QueueProp(commands, GlowOrbPrefab, "GuideLamp_3",
			Vector3(6.0f, 10.7f, 92.0f), Vector3(0.28f, 0.28f, 0.28f),
			WarmGlow());
		QueueProp(commands, GlowOrbPrefab, "GuideLamp_4",
			Vector3(-3.0f, 5.8f, 121.0f), Vector3(0.28f, 0.28f, 0.28f),
			WarmGlow());
		QueueProp(commands, GlowOrbPrefab, "GuideLamp_5",
			Vector3(3.0f, 5.8f, 121.0f), Vector3(0.28f, 0.28f, 0.28f),
			WarmGlow());
	}
