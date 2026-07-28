#ifndef RR2NW_SPARK_SUBJECT_STATE_H
#define RR2NW_SPARK_SUBJECT_STATE_H

class SimulationContext;

void SparkSubjectState_Link();
bool SparkSubjectState_TableReady(SimulationContext *context,
                                  int expectedCapacity);
int SparkSubjectState_Capacity();
int SparkSubjectState_LiveCount();

#endif
