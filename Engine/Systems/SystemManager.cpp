#include "SystemManager.h"
#include <chrono>
#include "Debug/Perf.h"


void Framework::SystemManager::IntializeAll()
{
	for (auto& sys : systems) {
		sys->Initialize();
	}
}

void Framework::SystemManager::UpdateAll(float dt)
{
	using clock = std::chrono::high_resolution_clock;
	for (auto& sys : systems) {
		const auto start = clock::now();
		sys->Update(dt);
		const double elapsedMs = std::chrono::duration<double, std::milli>(clock::now() - start).count();
		Framework::RecordSystemTiming(sys->GetName(), elapsedMs);
	}
}

void Framework::SystemManager::DrawAll()
{
	using clock = std::chrono::high_resolution_clock;
	for (auto& sys : systems) {
		const auto start = clock::now();
		sys->draw();
		const double elapsedMs = std::chrono::duration<double, std::milli>(clock::now() - start).count();
		Framework::RecordSystemTiming(sys->GetName(), elapsedMs);
	}
}

void Framework::SystemManager::ShutdownAll()
{
	for (auto& sys : systems) {
		sys->Shutdown();
	}
	systems.clear();
}

Framework::SystemManager::~SystemManager()
{
	ShutdownAll();
}