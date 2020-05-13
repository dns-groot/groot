#ifndef STRUCTURAL_TASK_CLASS_H
#define STRUCTURAL_TASK_CLASS_H

#include "label-graph.h"
#include "task.h"

class StructuralTask : public Task
{
  public:
    label::Graph::VertexDescriptor node_;
    string domain_name_;
    StructuralTask(label::Graph::VertexDescriptor, string);
    virtual string PrintTaskType();
    virtual void Process(const Context &, vector<boost::any> &);
};

#endif