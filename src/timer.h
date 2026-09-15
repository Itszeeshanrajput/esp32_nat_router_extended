void initializeRestartTimer();
void initializeKeepAliveTimer();
void initializePingWatchdog();
int watchdog_get_last_latency();
int watchdog_get_fail_count();
void restartByTimer();
void restartByTimerinS(int seconds);