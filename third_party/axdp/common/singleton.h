/*
 * created by lcd on 2019/1/7
 */
#ifndef SINGLETON_H_
#define SINGLETON_H_

#include "noncopiable.h"
#include <iostream>
#include <mutex>
#include <memory>
 
namespace astro
{
	template <class T>
	class Singleton : private NonCopiable
	{
	public:
		static T& GetInstance();
		static void DeInstance();
	protected:
		explicit Singleton() = default;
		virtual ~Singleton() = default;
	private:
		static std::mutex mutex_;
		static std::unique_ptr<T> s_instance_;
	};

	template <class T>
	std::mutex Singleton<T>::mutex_;

	template <class T>
	std::unique_ptr<T> Singleton<T>::s_instance_ = nullptr;

	template  <class T>
	T& Singleton<T>::GetInstance()
	{
		if (s_instance_.get() == nullptr)
		{
			std::unique_lock<std::mutex> lock(mutex_);
			if (s_instance_.get() == nullptr)
			{
				s_instance_ = std::unique_ptr<T>(new T);
			}
		}
		return *s_instance_.get();
	}
	template <class T>
	void Singleton<T>::DeInstance()
	{
		if (s_instance_.get() != nullptr)
		{
			s_instance_.reset();
			s_instance_ = nullptr;
		}
	}
}

#endif // !SINGLETON_H_

