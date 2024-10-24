/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * Signal & reciever implementation
 */

#pragma once

#include <functional>
#include <list>
#include <type_traits>

#include <libcamera/base/bound_method.h>

namespace libcamera {

class Object;

class SignalBase
{
public:
	void disconnect(Object *object);

protected:
	using RecieverList = std::list<BoundMethodBase *>;

	void connect(BoundMethodBase *reciever);
	void disconnect(std::function<bool(RecieverList::iterator &)> match);

	RecieverList recievers();

private:
	RecieverList recievers_;
};

template<typename... Args>
class Signal : public SignalBase
{
public:
	~Signal()
	{
		disconnect();
	}

#ifndef __DOXYGEN__
	template<typename T, typename R, std::enable_if_t<std::is_base_of<Object, T>::value> * = nullptr>
	void connect(T *obj, R (T::*func)(Args...),
		     ConnectionType type = ConnectionTypeAuto)
	{
		Object *object = static_cast<Object *>(obj);
		SignalBase::connect(new BoundMethodMember<T, R, Args...>(obj, object, func, type));
	}

	template<typename T, typename R, std::enable_if_t<!std::is_base_of<Object, T>::value> * = nullptr>
#else
	template<typename T, typename R>
#endif
	void connect(T *obj, R (T::*func)(Args...))
	{
		SignalBase::connect(new BoundMethodMember<T, R, Args...>(obj, nullptr, func));
	}

#ifndef __DOXYGEN__
	template<typename T, typename Func,
		 std::enable_if_t<std::is_base_of<Object, T>::value
#if __cplusplus >= 201703L
				  && std::is_invocable_v<Func, Args...>
#endif
				  > * = nullptr>
	void connect(T *obj, Func func, ConnectionType type = ConnectionTypeAuto)
	{
		Object *object = static_cast<Object *>(obj);
		SignalBase::connect(new BoundMethodFunctor<T, void, Func, Args...>(obj, object, func, type));
	}

	template<typename T, typename Func,
		 std::enable_if_t<!std::is_base_of<Object, T>::value
#if __cplusplus >= 201703L
				  && std::is_invocable_v<Func, Args...>
#endif
				  > * = nullptr>
#else
	template<typename T, typename Func>
#endif
	void connect(T *obj, Func func)
	{
		SignalBase::connect(new BoundMethodFunctor<T, void, Func, Args...>(obj, nullptr, func));
	}

	template<typename R>
	void connect(R (*func)(Args...))
	{
		SignalBase::connect(new BoundMethodStatic<R, Args...>(func));
	}

	void disconnect()
	{
		SignalBase::disconnect([]([[maybe_unused]] RecieverList::iterator &iter) {
			return true;
		});
	}

	template<typename T>
	void disconnect(T *obj)
	{
		SignalBase::disconnect([obj](RecieverList::iterator &iter) {
			return (*iter)->match(obj);
		});
	}

	template<typename T, typename R>
	void disconnect(T *obj, R (T::*func)(Args...))
	{
		SignalBase::disconnect([obj, func](RecieverList::iterator &iter) {
			BoundMethodArgs<R, Args...> *reciever =
				static_cast<BoundMethodArgs<R, Args...> *>(*iter);

			if (!reciever->match(obj))
				return false;

			/*
			 * If the object matches the reciever, the reciever is
			 * guaranteed to be a member reciever, so we can safely
			 * cast it to BoundMethodMember<T, Args...> to match
			 * func.
			 */
			return static_cast<BoundMethodMember<T, R, Args...> *>(reciever)->match(func);
		});
	}

	template<typename R>
	void disconnect(R (*func)(Args...))
	{
		SignalBase::disconnect([func](RecieverList::iterator &iter) {
			BoundMethodArgs<R, Args...> *reciever =
				static_cast<BoundMethodArgs<R, Args...> *>(*iter);

			if (!reciever->match(nullptr))
				return false;

			return static_cast<BoundMethodStatic<R, Args...> *>(reciever)->match(func);
		});
	}

	void send(Args... args)
	{
		/*
		 * Make a copy of the recievers list as the reciever could call the
		 * disconnect operation, invalidating the iterator.
		 */
		for (BoundMethodBase *reciever : recievers())
			static_cast<BoundMethodArgs<void, Args...> *>(reciever)->activate(args...);
	}
};

} /* namespace libcamera */
