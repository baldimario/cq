#include "date_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* date utility functions */

int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && is_leap_year(year)) return 29;
    return days[month - 1];
}

int is_valid_date(int year, int month, int day) {
    if (year < 1000 || year > 9999) return 0;
    if (month < 1 || month > 12) return 0;
    if (day < 1) return 0;
    return day <= days_in_month(year, month);
}

int parse_date_format(const char* str, DateValue* date, DateFormat format) {
    if (!str || !date) return 0;
    
    int y = 0, m = 0, d = 0;
    
    switch (format) {
        case DATE_FORMAT_ISO:
            // YYYY-MM-DD
            if (sscanf(str, "%d-%d-%d", &y, &m, &d) == 3) {
                if (is_valid_date(y, m, d)) {
                    date->year = y;
                    date->month = m;
                    date->day = d;
                    return 1;
                }
            }
            break;
            
        case DATE_FORMAT_US:
            // MM/DD/YYYY
            if (sscanf(str, "%d/%d/%d", &m, &d, &y) == 3) {
                if (is_valid_date(y, m, d)) {
                    date->year = y;
                    date->month = m;
                    date->day = d;
                    return 1;
                }
            }
            break;
            
        case DATE_FORMAT_EU:
            // DD/MM/YYYY
            if (sscanf(str, "%d/%d/%d", &d, &m, &y) == 3) {
                if (is_valid_date(y, m, d)) {
                    date->year = y;
                    date->month = m;
                    date->day = d;
                    return 1;
                }
            }
            break;
            
        case DATE_FORMAT_COMPACT:
            // YYYYMMDD
            if (sscanf(str, "%8d", &y) == 1) {
                d = y % 100;
                y /= 100;
                m = y % 100;
                y /= 100;
                if (is_valid_date(y, m, d)) {
                    date->year = y;
                    date->month = m;
                    date->day = d;
                    return 1;
                }
            }
            break;
    }
    
    return 0;
}

int parse_date(const char* str, DateValue* date) {
    if (!str || !date) return 0;
    
    // try ISO format first (most common in SQL)
    if (parse_date_format(str, date, DATE_FORMAT_ISO)) return 1;
    
    // try other formats
    if (parse_date_format(str, date, DATE_FORMAT_US)) return 1;
    if (parse_date_format(str, date, DATE_FORMAT_EU)) return 1;
    if (parse_date_format(str, date, DATE_FORMAT_COMPACT)) return 1;
    
    return 0;
}

char* format_date(DateValue date, DateFormat format) {
    char* result = malloc(32);
    if (!result) return NULL;
    
    switch (format) {
        case DATE_FORMAT_ISO:
            snprintf(result, 32, "%04d-%02d-%02d", date.year, date.month, date.day);
            break;
        case DATE_FORMAT_US:
            snprintf(result, 32, "%02d/%02d/%04d", date.month, date.day, date.year);
            break;
        case DATE_FORMAT_EU:
            snprintf(result, 32, "%02d/%02d/%04d", date.day, date.month, date.year);
            break;
        case DATE_FORMAT_COMPACT:
            snprintf(result, 32, "%04d%02d%02d", date.year, date.month, date.day);
            break;
        default:
            snprintf(result, 32, "%04d-%02d-%02d", date.year, date.month, date.day);
    }
    
    return result;
}

long date_to_days(DateValue date) {
    // algorithm: count days since 1970-01-01 (Unix epoch)
    long days = 0;
    
    // add days for complete years
    for (int y = 1970; y < date.year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }
    
    // handle negative years (before 1970)
    for (int y = 1969; y >= date.year; y--) {
        days -= is_leap_year(y) ? 366 : 365;
    }
    
    // add days for complete months in current year
    for (int m = 1; m < date.month; m++) {
        days += days_in_month(date.year, m);
    }
    
    // add remaining days
    days += date.day - 1;
    
    return days;
}

DateValue days_to_date(long days) {
    DateValue result;
    
    // start from epoch
    result.year = 1970;
    result.month = 1;
    result.day = 1;
    
    // handle negative days (before epoch)
    if (days < 0) {
        while (days < 0) {
            result.year--;
            int year_days = is_leap_year(result.year) ? 366 : 365;
            days += year_days;
        }
    } else {
        // add complete years
        while (1) {
            int year_days = is_leap_year(result.year) ? 366 : 365;
            if (days >= year_days) {
                days -= year_days;
                result.year++;
            } else {
                break;
            }
        }
    }
    
    // add complete months
    while (days >= days_in_month(result.year, result.month)) {
        days -= days_in_month(result.year, result.month);
        result.month++;
        if (result.month > 12) {
            result.month = 1;
            result.year++;
        }
    }
    
    // remaining days
    result.day += (int)days;
    
    return result;
}

int compare_dates(DateValue d1, DateValue d2) {
    if (d1.year != d2.year) return d1.year - d2.year;
    if (d1.month != d2.month) return d1.month - d2.month;
    return d1.day - d2.day;
}

DateValue date_add_days(DateValue date, int days) {
    long total_days = date_to_days(date);
    total_days += days;
    return days_to_date(total_days);
}

DateValue date_add_months(DateValue date, int months) {
    int new_month = date.month + months;
    int year_offset = 0;
    
    // handle month overflow/underflow
    while (new_month > 12) {
        new_month -= 12;
        year_offset++;
    }
    while (new_month < 1) {
        new_month += 12;
        year_offset--;
    }
    
    DateValue result = date;
    result.year += year_offset;
    result.month = new_month;
    
    // adjust day if it exceeds days in new month
    int max_day = days_in_month(result.year, result.month);
    if (result.day > max_day) {
        result.day = max_day;
    }
    
    return result;
}

DateValue date_add_years(DateValue date, int years) {
    DateValue result = date;
    result.year += years;
    
    // handle Feb 29 on non-leap year
    if (result.month == 2 && result.day == 29 && !is_leap_year(result.year)) {
        result.day = 28;
    }
    
    return result;
}

long date_diff_days(DateValue d1, DateValue d2) {
    return date_to_days(d1) - date_to_days(d2);
}

long date_diff_months(DateValue d1, DateValue d2) {
    return (d1.year - d2.year) * 12 + (d1.month - d2.month);
}

long date_diff_years(DateValue d1, DateValue d2) {
    return d1.year - d2.year;
}

DateValue current_date(void) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    DateValue result;
    result.year = tm_info->tm_year + 1900;
    result.month = tm_info->tm_mon + 1;
    result.day = tm_info->tm_mday;
    
    return result;
}

int date_get_year(DateValue date) {
    return date.year;
}

int date_get_month(DateValue date) {
    return date.month;
}

int date_get_day(DateValue date) {
    return date.day;
}

int date_get_dayofweek(DateValue date) {
    // Zeller's congruence algorithm
    int y = date.year;
    int m = date.month;
    int d = date.day;
    
    if (m < 3) {
        m += 12;
        y--;
    }
    
    int c = y / 100;
    y = y % 100;
    
    int w = (d + (13 * (m + 1)) / 5 + y + y / 4 + c / 4 - 2 * c) % 7;
    
    // convert to 0=Sunday, 6=Saturday
    return (w + 6) % 7;
}

int date_get_dayofyear(DateValue date) {
    int day = 0;
    for (int m = 1; m < date.month; m++) {
        day += days_in_month(date.year, m);
    }
    day += date.day;
    return day;
}
