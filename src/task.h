#pragma once
#ifndef TASK_H
#define TASK_H

#include "interpretation-properties.h"

class Task
{
  public:
    virtual string PrintTaskType() = 0;
    virtual void Process(const Context &, vector<boost::any> &) = 0;
};

#endif