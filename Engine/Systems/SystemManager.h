#pragma once
#include <memory>
#include <vector>
#include <type_traits>
#include "Common/System.h"

namespace Framework {
	class SystemManager {
	public:
		SystemManager() = default;
		~SystemManager();

		// only one system so remove copy/copy assign and move/move assign
		SystemManager(const SystemManager&) = delete;
		SystemManager& operator=(const SystemManager&) = delete;

		SystemManager(SystemManager&&) = delete;
		SystemManager& operator=(SystemManager&&) = delete;

		//intialize all system
		void IntializeAll();

		//Update all system
		void UpdateAll(float dt);

		void DrawAll();
		void ShutdownAll();

		template<typename T ,typename... Args>
		T* RegisterSystem(Args&&... args) {
			static_assert(std::is_base_of_v<ISystem, T>, "System must be derive from ISystem");
			auto sys = std::make_unique<T>(std::forward<Args>(args)...);
			T* ptr = sys.get();
			systems.emplace_back(std::move(sys));
			return ptr;
		}

	private:

		std::vector<std::unique_ptr<ISystem>> systems{};

	};

}