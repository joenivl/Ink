// Variabelen die deep sleep overleven (RTC-geheugen). Hier en niet in
// ink_kalender.h, want ESPHome neemt die header in elk bronbestand op.
#include "ink_kalender.h"

#include <cmath>
#include <esp_attr.h>

namespace ink {

RTC_DATA_ATTR WeerGeheugen rtc_weer = {};
RTC_DATA_ATTR KlaarGeheugen rtc_klaar = {};
RTC_DATA_ATTR uint32_t rtc_inhoud_hash = 0;
RTC_DATA_ATTR time_t rtc_slaap_begin = 0;
RTC_DATA_ATTR uint8_t rtc_snelle_touch_wekkers = 0;
RTC_DATA_ATTR float rtc_accu = NAN;

}  // namespace ink
