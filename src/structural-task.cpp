#include "structural-task.h"

StructuralTask::StructuralTask(label::Graph::VertexDescriptor node, string domain) :node_(node), domain_name_(domain) {}

string StructuralTask::PrintTaskType()
{
	return "Task: Structural";
}

void StructuralTask::Process(const Context& context, vector<boost::any>& variadic_arguments)
{
	Job* current_job = boost::any_cast<Job*>(variadic_arguments[0]);
	label::Graph* label_graph = boost::any_cast<label::Graph*>(variadic_arguments[1]);
	label_graph->CheckStructuralDelegationConsistency(domain_name_, node_, context, *current_job);
}