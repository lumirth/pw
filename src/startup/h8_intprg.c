
/* These unused interrupt vectors return immediately. Peripheral modules define
 * the active handlers. */

#pragma section IntPRG

#pragma interrupt(NMIInterrupt(vect = 7))
void NMIInterrupt(void)
{
}

#pragma interrupt(TRAPA0Interrupt(vect = 8))
void TRAPA0Interrupt(void)
{
}

#pragma interrupt(TRAPA1Interrupt(vect = 9))
void TRAPA1Interrupt(void)
{
}

#pragma interrupt(TRAPA2Interrupt(vect = 10))
void TRAPA2Interrupt(void)
{
}

#pragma interrupt(TRAPA3Interrupt(vect = 11))
void TRAPA3Interrupt(void)
{
}

#pragma interrupt(DirectTransitionInterrupt(vect = 13))
void DirectTransitionInterrupt(void)
{
}

#pragma interrupt(Comparator0Interrupt(vect = 21))
void Comparator0Interrupt(void)
{
}

#pragma interrupt(Comparator1Interrupt(vect = 22))
void Comparator1Interrupt(void)
{
}

#pragma interrupt(RtcWeekInterrupt(vect = 29))
void RtcWeekInterrupt(void)
{
}

#pragma interrupt(RtcFreeRunningInterrupt(vect = 30))
void RtcFreeRunningInterrupt(void)
{
}

#pragma interrupt(WatchdogInterrupt(vect = 31))
void WatchdogInterrupt(void)
{
}

#pragma interrupt(EventOverflowInterrupt(vect = 32))
void EventOverflowInterrupt(void)
{
}

#pragma interrupt(SsuI2cInterrupt(vect = 34))
void SsuI2cInterrupt(void)
{
}
