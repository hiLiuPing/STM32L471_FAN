#ifndef __SYSTEM_POWER_H__
#define __SYSTEM_POWER_H__

#ifdef __cplusplus
extern "C" {
#endif

void SystemPower_RequestShutdown(void);
void SystemPower_Service(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_POWER_H__ */
