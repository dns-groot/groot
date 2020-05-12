#include "structural-task.h"

StructuralTask::StructuralTask(label::Graph::VertexDescriptor node, string domain) :node_(node), domain_name_(domain) {}

string StructuralTask::PrintTaskType()
{
	return "Task: Structural";
}
