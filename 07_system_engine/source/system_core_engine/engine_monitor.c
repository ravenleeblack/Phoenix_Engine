#include "engine_globals.h"

/* monitor_01 is the primary monitor, monitor_02 is the first other monitor. enumeration order does not matter. */
BOOL CALLBACK collect_monitor_layout(HMONITOR monitor, HDC monitor_dc, LPRECT monitor_rect, LPARAM user_data)
{
    (void)monitor_dc;
    (void)monitor_rect;

    monitor_layout* layout = (monitor_layout*)user_data;
    MONITORINFO info = {0};
    info.cbSize = sizeof(MONITORINFO);

    if (GetMonitorInfoA(monitor, &info))
    {
        if (info.dwFlags & MONITORINFOF_PRIMARY)
        {
            layout->monitor_01_rect = info.rcWork;
            layout->has_primary = true;
        }
        else if (!layout->has_secondary)
        {
            layout->monitor_02_rect = info.rcWork;
            layout->has_secondary = true;
        }
        layout->monitor_count++;
    }
    return TRUE;
}