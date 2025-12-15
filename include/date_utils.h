#ifndef DATE_UTILS_H
#define DATE_UTILS_H

#include "csv_reader.h"
#include <time.h>

/* date format types */
typedef enum {
    DATE_FORMAT_ISO,       // YYYY-MM-DD (default, SQL standard)
    DATE_FORMAT_US,        // MM/DD/YYYY
    DATE_FORMAT_EU,        // DD/MM/YYYY
    DATE_FORMAT_COMPACT    // YYYYMMDD
} DateFormat;

/* parse date string to DateValue, returns 1 on success, 0 on failure */
int parse_date(const char* str, DateValue* date);

/* parse date with specific format */
int parse_date_format(const char* str, DateValue* date, DateFormat format);

/* format DateValue to string (caller must free) */
char* format_date(DateValue date, DateFormat format);

/* validate date */
int is_valid_date(int year, int month, int day);

/* check if year is leap year */
int is_leap_year(int year);

/* get number of days in month */
int days_in_month(int year, int month);

/* convert date to days since Unix epoch (1970-01-01) */
long date_to_days(DateValue date);

/* convert days since epoch to date */
DateValue days_to_date(long days);

/* compare two dates, returns -1, 0, or 1 */
int compare_dates(DateValue d1, DateValue d2);

/* date arithmetic: add days/months/years */
DateValue date_add_days(DateValue date, int days);
DateValue date_add_months(DateValue date, int months);
DateValue date_add_years(DateValue date, int years);

/* date difference in days/months/years */
long date_diff_days(DateValue d1, DateValue d2);
long date_diff_months(DateValue d1, DateValue d2);
long date_diff_years(DateValue d1, DateValue d2);

/* get current date */
DateValue current_date(void);

/* extract components */
int date_get_year(DateValue date);
int date_get_month(DateValue date);
int date_get_day(DateValue date);
int date_get_dayofweek(DateValue date);  // 0=Sunday, 6=Saturday
int date_get_dayofyear(DateValue date);  // 1-366

#endif /* DATE_UTILS_H */
