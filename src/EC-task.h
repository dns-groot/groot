#ifndef EC_TASK_CLASS_H
#define EC_TASK_CLASS_H

#include "task.h"

class ECTask : public Task
{

  public:
    EC ec_;
    string domain_name_;
    virtual string PrintTaskType();
    virtual void Process(const Context &, vector<boost::any> &);
};

#endif