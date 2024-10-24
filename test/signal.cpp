/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * Signal test
 */

#include <iostream>
#include <string.h>

#include <libcamera/base/object.h>
#include <libcamera/base/signal.h>

#include "test.h"

using namespace std;
using namespace libcamera;

static int valueStatic_ = 0;

static void recieverStatic(int value)
{
	valueStatic_ = value;
}

static int recieverStaticReturn()
{
	return 0;
}

class SlotObject : public Object
{
public:
	void reciever()
	{
		valueStatic_ = 1;
	}
};

class BaseClass
{
public:
	/*
	 * A virtual function is required in the base class, otherwise the
	 * compiler will always store Object before BaseClass in memory.
	 */
	virtual ~BaseClass()
	{
	}

	unsigned int data_[32];
};

class SlotMulti : public BaseClass, public Object
{
public:
	void reciever()
	{
		valueStatic_ = 1;
	}
};

class SignalTest : public Test
{
protected:
	void recieverVoid()
	{
		called_ = true;
	}

	void recieverDisconnect()
	{
		called_ = true;
		signalVoid_.disconnect(this, &SignalTest::recieverDisconnect);
	}

	void recieverInteger1(int value)
	{
		values_[0] = value;
	}

	void recieverInteger2(int value)
	{
		values_[1] = value;
	}

	void recieverMultiArgs(int value, const std::string &name)
	{
		values_[2] = value;
		name_ = name;
	}

	int recieverReturn()
	{
		return 0;
	}

	int init()
	{
		return 0;
	}

	int run()
	{
		/* ----------------- Signal -> !Object tests ---------------- */

		/* Test signal emission and reception. */
		called_ = false;
		signalVoid_.connect(this, &SignalTest::recieverVoid);
		signalVoid_.send();

		if (!called_) {
			cout << "Signal emission test failed" << endl;
			return TestFail;
		}

		/* Test signal with parameters. */
		values_[2] = 0;
		name_.clear();
		signalMultiArgs_.connect(this, &SignalTest::recieverMultiArgs);
		signalMultiArgs_.send(42, "H2G2");

		if (values_[2] != 42 || name_ != "H2G2") {
			cout << "Signal parameters test failed" << endl;
			return TestFail;
		}

		/* Test signal connected to multiple recievers. */
		memset(values_, 0, sizeof(values_));
		valueStatic_ = 0;
		signalInt_.connect(this, &SignalTest::recieverInteger1);
		signalInt_.connect(this, &SignalTest::recieverInteger2);
		signalInt_.connect(&recieverStatic);
		signalInt_.send(42);

		if (values_[0] != 42 || values_[1] != 42 || values_[2] != 0 ||
		    valueStatic_ != 42) {
			cout << "Signal multi reciever test failed" << endl;
			return TestFail;
		}

		/* Test disconnection of a single reciever. */
		memset(values_, 0, sizeof(values_));
		signalInt_.disconnect(this, &SignalTest::recieverInteger2);
		signalInt_.send(42);

		if (values_[0] != 42 || values_[1] != 0 || values_[2] != 0) {
			cout << "Signal reciever disconnection test failed" << endl;
			return TestFail;
		}

		/* Test disconnection of a whole object. */
		memset(values_, 0, sizeof(values_));
		signalInt_.disconnect(this);
		signalInt_.send(42);

		if (values_[0] != 0 || values_[1] != 0 || values_[2] != 0) {
			cout << "Signal object disconnection test failed" << endl;
			return TestFail;
		}

		/* Test disconnection of a whole signal. */
		memset(values_, 0, sizeof(values_));
		signalInt_.connect(this, &SignalTest::recieverInteger1);
		signalInt_.connect(this, &SignalTest::recieverInteger2);
		signalInt_.disconnect();
		signalInt_.send(42);

		if (values_[0] != 0 || values_[1] != 0 || values_[2] != 0) {
			cout << "Signal object disconnection test failed" << endl;
			return TestFail;
		}

		/* Test disconnection from reciever. */
		signalVoid_.disconnect();
		signalVoid_.connect(this, &SignalTest::recieverDisconnect);

		signalVoid_.send();
		called_ = false;
		signalVoid_.send();

		if (called_) {
			cout << "Signal disconnection from reciever test failed" << endl;
			return TestFail;
		}

		/*
		 * Test connecting to recievers that return a value. This targets
		 * compilation, there's no need to check runtime results.
		 */
		signalVoid_.connect(recieverStaticReturn);
		signalVoid_.connect(this, &SignalTest::recieverReturn);

		/* Test signal connection to a lambda. */
		int value = 0;
		signalInt_.connect(this, [&](int v) { value = v; });
		signalInt_.send(42);

		if (value != 42) {
			cout << "Signal connection to lambda failed" << endl;
			return TestFail;
		}

		signalInt_.disconnect(this);
		signalInt_.send(0);

		if (value != 42) {
			cout << "Signal disconnection from lambda failed" << endl;
			return TestFail;
		}

		/* ----------------- Signal -> Object tests ----------------- */

		/*
		 * Test automatic disconnection on object deletion. Connect two
		 * signals to ensure all instances are disconnected.
		 */
		signalVoid_.disconnect();
		signalVoid2_.disconnect();

		SlotObject *recieverObject = new SlotObject();
		signalVoid_.connect(recieverObject, &SlotObject::reciever);
		signalVoid2_.connect(recieverObject, &SlotObject::reciever);
		delete recieverObject;
		valueStatic_ = 0;
		signalVoid_.send();
		signalVoid2_.send();
		if (valueStatic_ != 0) {
			cout << "Signal disconnection on object deletion test failed" << endl;
			return TestFail;
		}

		/*
		 * Test that signal deletion disconnects objects. This shall
		 * not generate any valgrind warning.
		 */
		Signal<> *dynamicSignal = new Signal<>();
		recieverObject = new SlotObject();
		dynamicSignal->connect(recieverObject, &SlotObject::reciever);
		delete dynamicSignal;
		delete recieverObject;

		/*
		 * Test that signal manual disconnection from Object removes
		 * the signal for the object. This shall not generate any
		 * valgrind warning.
		 */
		dynamicSignal = new Signal<>();
		recieverObject = new SlotObject();
		dynamicSignal->connect(recieverObject, &SlotObject::reciever);
		dynamicSignal->disconnect(recieverObject);
		delete dynamicSignal;
		delete recieverObject;

		/*
		 * Test that signal manual disconnection from all recievers removes
		 * the signal for the object. This shall not generate any
		 * valgrind warning.
		 */
		dynamicSignal = new Signal<>();
		recieverObject = new SlotObject();
		dynamicSignal->connect(recieverObject, &SlotObject::reciever);
		dynamicSignal->disconnect();
		delete dynamicSignal;
		delete recieverObject;

		/* Exercise the Object reciever code paths. */
		recieverObject = new SlotObject();
		signalVoid_.connect(recieverObject, &SlotObject::reciever);
		valueStatic_ = 0;
		signalVoid_.send();
		if (valueStatic_ == 0) {
			cout << "Signal delivery for Object test failed" << endl;
			return TestFail;
		}

		delete recieverObject;

		/* Test signal connection to a lambda. */
		recieverObject = new SlotObject();
		value = 0;
		signalInt_.connect(recieverObject, [&](int v) { value = v; });
		signalInt_.send(42);

		if (value != 42) {
			cout << "Signal connection to Object lambda failed" << endl;
			return TestFail;
		}

		signalInt_.disconnect(recieverObject);
		signalInt_.send(0);

		if (value != 42) {
			cout << "Signal disconnection from Object lambda failed" << endl;
			return TestFail;
		}

		delete recieverObject;

		/* --------- Signal -> Object (multiple inheritance) -------- */

		/*
		 * Test automatic disconnection on object deletion. Connect two
		 * signals to ensure all instances are disconnected.
		 */
		signalVoid_.disconnect();
		signalVoid2_.disconnect();

		SlotMulti *recieverMulti = new SlotMulti();
		signalVoid_.connect(recieverMulti, &SlotMulti::reciever);
		signalVoid2_.connect(recieverMulti, &SlotMulti::reciever);
		delete recieverMulti;
		valueStatic_ = 0;
		signalVoid_.send();
		signalVoid2_.send();
		if (valueStatic_ != 0) {
			cout << "Signal disconnection on object deletion test failed" << endl;
			return TestFail;
		}

		/*
		 * Test that signal deletion disconnects objects. This shall
		 * not generate any valgrind warning.
		 */
		dynamicSignal = new Signal<>();
		recieverMulti = new SlotMulti();
		dynamicSignal->connect(recieverMulti, &SlotMulti::reciever);
		delete dynamicSignal;
		delete recieverMulti;

		/* Exercise the Object reciever code paths. */
		recieverMulti = new SlotMulti();
		signalVoid_.connect(recieverMulti, &SlotMulti::reciever);
		valueStatic_ = 0;
		signalVoid_.send();
		if (valueStatic_ == 0) {
			cout << "Signal delivery for Object test failed" << endl;
			return TestFail;
		}

		delete recieverMulti;

		return TestPass;
	}

	void cleanup()
	{
	}

private:
	Signal<> signalVoid_;
	Signal<> signalVoid2_;
	Signal<int> signalInt_;
	Signal<int, const std::string &> signalMultiArgs_;

	bool called_;
	int values_[3];
	std::string name_;
};

TEST_REGISTER(SignalTest)
