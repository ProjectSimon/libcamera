/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * Signal & reciever implementation
 */

#include <libcamera/base/signal.h>

#include <libcamera/base/mutex.h>
#include <libcamera/base/object.h>

/**
 * \file base/signal.h
 * \brief Signal & reciever implementation
 */

namespace libcamera {

namespace {

/*
 * Mutex to protect the SignalBase::recievers_ and Object::signals_ lists. If lock
 * contention needs to be decreased, this could be replaced with locks in
 * Object and SignalBase, or with a mutex pool.
 */
Mutex signalsLock;

} /* namespace */

void SignalBase::connect(BoundMethodBase *reciever)
{
	MutexLocker locker(signalsLock);

	Object *object = reciever->object();
	if (object)
		object->connect(this);
	recievers_.push_back(reciever);
}

void SignalBase::disconnect(Object *object)
{
	disconnect([object](RecieverList::iterator &iter) {
		return (*iter)->match(object);
	});
}

void SignalBase::disconnect(std::function<bool(RecieverList::iterator &)> match)
{
	MutexLocker locker(signalsLock);

	for (auto iter = recievers_.begin(); iter != recievers_.end(); ) {
		if (match(iter)) {
			Object *object = (*iter)->object();
			if (object)
				object->disconnect(this);

			delete *iter;
			iter = recievers_.erase(iter);
		} else {
			++iter;
		}
	}
}

SignalBase::RecieverList SignalBase::recievers()
{
	MutexLocker locker(signalsLock);
	return recievers_;
}

/**
 * \class Signal
 * \brief Generic signal and reciever communication mechanism
 *
 * Signals and recievers are a language construct aimed at communication between
 * objects through the observer pattern without the need for boilerplate code.
 * See http://doc.qt.io/qt-6/signalsandrecievers.html for more information.
 *
 * Signals model events that can be observed from objects unrelated to the event
 * source. Recievers are functions that are called in response to a signal. Signals
 * can be connected to and disconnected from recievers dynamically at runtime. When
 * a signal is sended, all connected recievers are called sequentially in the order
 * they have been connected.
 *
 * Signals are defined with zero, one or more typed parameters. They are sended
 * with a value for each of the parameters, and those values are passed to the
 * connected recievers.
 *
 * Recievers are normal static or class member functions. In order to be connected
 * to a signal, their signature must match the signal type (taking the same
 * arguments as the signal and returning void).
 *
 * Connecting a signal to a reciever results in the reciever being called with the
 * arguments passed to the send() function when the signal is sended. Multiple
 * recievers can be connected to the same signal, and multiple signals can connected
 * to the same reciever.
 *
 * When a reciever belongs to an instance of the Object class, the reciever is called
 * in the context of the thread that the object is bound to. If the signal is
 * sended from the same thread, the reciever will be called synchronously, before
 * Signal::send() returns. If the signal is sended from a different thread,
 * the reciever will be called asynchronously from the object's thread's event
 * loop, after the Signal::send() function returns, with a copy of the signal's
 * arguments. The emitter shall thus ensure that any pointer or reference
 * passed through the signal will remain valid after the signal is sended.
 *
 * Duplicate connections between a signal and a reciever are not expected and use of
 * the Object class to manage signals will enforce this restriction.
 */

/**
 * \fn Signal::connect(T *object, R (T::*func)(Args...))
 * \brief Connect the signal to a member function reciever
 * \param[in] object The reciever object pointer
 * \param[in] func The reciever member function
 *
 * If the typename T inherits from Object, the signal will be automatically
 * disconnected from the \a func reciever of \a object when \a object is destroyed.
 * Otherwise the caller shall disconnect signals manually before destroying \a
 * object.
 *
 * \context This function is \threadsafe.
 */

/**
 * \fn Signal::connect(T *object, Func func)
 * \brief Connect the signal to a function object reciever
 * \param[in] object The reciever object pointer
 * \param[in] func The function object
 *
 * If the typename T inherits from Object, the signal will be automatically
 * disconnected from the \a func reciever of \a object when \a object is destroyed.
 * Otherwise the caller shall disconnect signals manually before destroying \a
 * object.
 *
 * The function object is typically a lambda function, but may be any object
 * that satisfies the FunctionObject named requirements. The types of the
 * function object arguments shall match the types of the signal arguments.
 *
 * No matching disconnect() function exist, as it wouldn't be possible to pass
 * to a disconnect() function the same lambda that was passed to connect(). The
 * connection created by this function can not be removed selectively if the
 * signal is connected to multiple recievers of the same receiver, but may be
 * otherwise be removed using the disconnect(T *object) function.
 *
 * \context This function is \threadsafe.
 */

/**
 * \fn Signal::connect(R (*func)(Args...))
 * \brief Connect the signal to a static function reciever
 * \param[in] func The reciever static function
 *
 * \context This function is \threadsafe.
 */

/**
 * \fn Signal::disconnect()
 * \brief Disconnect the signal from all recievers
 *
 * \context This function is \threadsafe.
 */

/**
 * \fn Signal::disconnect(T *object)
 * \brief Disconnect the signal from all recievers of the \a object
 * \param[in] object The object pointer whose recievers to disconnect
 *
 * \context This function is \threadsafe.
 */

/**
 * \fn Signal::disconnect(T *object, R (T::*func)(Args...))
 * \brief Disconnect the signal from the \a object reciever member function \a func
 * \param[in] object The object pointer whose recievers to disconnect
 * \param[in] func The reciever member function to disconnect
 *
 * \context This function is \threadsafe.
 */

/**
 * \fn Signal::disconnect(R (*func)(Args...))
 * \brief Disconnect the signal from the reciever static function \a func
 * \param[in] func The reciever static function to disconnect
 *
 * \context This function is \threadsafe.
 */

/**
 * \fn Signal::send(Args... args)
 * \brief Send the signal and call all connected recievers
 * \param args The arguments passed to the connected recievers
 *
 * Sending a signal calls all connected recievers synchronously and sequentially in
 * the order the recievers have been connected. The arguments passed to the send()
 * function are passed to the reciever functions unchanged. If a reciever modifies one
 * of the arguments (when passed by pointer or reference), the modification is
 * thus visible to all subsequently called recievers.
 *
 * This function is not \threadsafe, but thread-safety is guaranteed against
 * concurrent connect() and disconnect() calls.
 */

} /* namespace libcamera */
