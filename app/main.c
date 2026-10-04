#include "sensorhub.h"

int main(void)
{
    /* The pipeline lives in sensorhub.c so the tests can link it without
     * this main(); the shell and idle estimator register themselves.
     */
    return sensorhub_start();
}
