#pragma once

#include <functional>
#include <vector>

namespace WebCraft {
	namespace Util {
// TODO: Do this
		template <typename T>
		class Event {
		public:
			Event() = default;
			~Event() = default;

			void operator+=(const std::function<void(T)>& callback) {
				m_Callbacks.push_back(callback);
			}

			void operator-=(const std::function<void(T)>& callback) {
				m_Callbacks.erase(std::remove(m_Callbacks.begin(), m_Callbacks.end(), callback), m_Callbacks.end());
			}

			void Invoke(T arg) {
				for (auto& callback : m_Callbacks) {
					callback(arg);
				}
			}

		private:
			std::vector<std::function<void(T)>> m_Callbacks;
		};

		template <>
		class Event<void> {
		public:
			Event() = default;
			~Event() = default;

			void operator+=(const std::function<void()>& callback) {
				m_Callbacks.push_back(callback);
			}

			void operator-=(const std::function<void()>& callback) {
				m_Callbacks.erase(std::remove(m_Callbacks.begin(), m_Callbacks.end(), callback), m_Callbacks.end());
			}

			void Invoke() {
				for (auto& callback : m_Callbacks) {
					callback();
				}
			}

		private:
			std::vector<std::function<void()>> m_Callbacks;
		};
	}
}