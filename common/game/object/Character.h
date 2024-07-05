#pragma once

#include "Object.h"

class Character : public Object
{
private:

public:
	virtual void Update(const utility::Milliseconds current_time_epoch, const utility::Milliseconds delta) override {}
};