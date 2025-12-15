#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>
#include "evaluator.h"
#include "evaluator/evaluator_functions.h"
#include "utils.h"
#include "date_utils.h"

/* helper function to transform string case */
static char* transform_string_case(const char* str, bool to_upper) {
    if (!str) return NULL;
    
    char* result = strdup(str);
    for (int i = 0; result[i]; i++) {
        result[i] = to_upper ? toupper(result[i]) : tolower(result[i]);
    }
    return result;
}

/* evaluate scalar functions, handles concat, lower, upper, length, substring, replace, coalesce, power, sqrt, ceil, floor, round, abs, exp, ln, mod */
Value evaluate_scalar_function(const char* func_name, Value* args, int arg_count) {
    Value result;
    result.type = VALUE_TYPE_NULL;
    
    if (arg_count < 1) return result;
    
    // CONCAT
    if (strcasecmp(func_name, "CONCAT") == 0) {
        char buffer[1024] = "";
        for (int i = 0; i < arg_count; i++) {
            if (args[i].type == VALUE_TYPE_STRING && args[i].string_value) {
                strcat(buffer, args[i].string_value);
            } else if (args[i].type == VALUE_TYPE_INTEGER) {
                char temp[64];
                snprintf(temp, sizeof(temp), "%lld", args[i].int_value);
                strcat(buffer, temp);
            } else if (args[i].type == VALUE_TYPE_DOUBLE) {
                char temp[64];
                snprintf(temp, sizeof(temp), "%.2f", args[i].double_value);
                strcat(buffer, temp);
            }
        }
        result.type = VALUE_TYPE_STRING;
        result.string_value = strdup(buffer);
        return result;
    }
    
    // LOWER
    if (strcasecmp(func_name, "LOWER") == 0) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value) {
            result.type = VALUE_TYPE_STRING;
            result.string_value = transform_string_case(args[0].string_value, false);
        }
        return result;
    }
    
    // UPPER
    if (strcasecmp(func_name, "UPPER") == 0) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value) {
            result.type = VALUE_TYPE_STRING;
            result.string_value = transform_string_case(args[0].string_value, true);
        }
        return result;
    }
    
    // LENGTH
    if (strcasecmp(func_name, "LENGTH") == 0) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = strlen(args[0].string_value);
        }
        return result;
    }
    
    // SUBSTRING(str, start, length)
    if (strcasecmp(func_name, "SUBSTRING") == 0 && arg_count >= 3) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value &&
            args[1].type == VALUE_TYPE_INTEGER && args[2].type == VALUE_TYPE_INTEGER) {
            
            int start = args[1].int_value - 1; // convert to 0-indexed
            int length = args[2].int_value;
            const char* str = args[0].string_value;
            int str_len = strlen(str);
            
            if (start < 0) start = 0;
            if (start >= str_len) {
                result.type = VALUE_TYPE_STRING;
                result.string_value = strdup("");
                return result;
            }
            
            if (start + length > str_len) {
                length = str_len - start;
            }
            
            char* substr = malloc(length + 1);
            strncpy(substr, str + start, length);
            substr[length] = '\0';
            
            result.type = VALUE_TYPE_STRING;
            result.string_value = substr;
        }
        return result;
    }
    
    // REPLACE(str, from, to)
    if (strcasecmp(func_name, "REPLACE") == 0 && arg_count >= 3) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value &&
            args[1].type == VALUE_TYPE_STRING && args[1].string_value &&
            args[2].type == VALUE_TYPE_STRING && args[2].string_value) {
            
            const char* str = args[0].string_value;
            const char* from = args[1].string_value;
            const char* to = args[2].string_value;
            
            int from_len = strlen(from);
            int to_len = strlen(to);
            
            if (from_len == 0) {
                result.type = VALUE_TYPE_STRING;
                result.string_value = strdup(str);
                return result;
            }
            
            // count occurrences
            int count = 0;
            const char* pos = str;
            while ((pos = strstr(pos, from)) != NULL) {
                count++;
                pos += from_len;
            }
            
            // allocate result buffer
            int result_len = strlen(str) + count * (to_len - from_len);
            char* new_str = malloc(result_len + 1);
            char* dest = new_str;
            
            pos = str;
            while (*pos) {
                if (strncmp(pos, from, from_len) == 0) {
                    strcpy(dest, to);
                    dest += to_len;
                    pos += from_len;
                } else {
                    *dest++ = *pos++;
                }
            }
            *dest = '\0';
            
            result.type = VALUE_TYPE_STRING;
            result.string_value = new_str;
        }
        return result;
    }
    
    // COALESCE
    if (strcasecmp(func_name, "COALESCE") == 0) {
        for (int i = 0; i < arg_count; i++) {
            if (args[i].type != VALUE_TYPE_NULL) {
                // deep copy the value to avoid freeing shared pointers
                result.type = args[i].type;
                if (args[i].type == VALUE_TYPE_STRING && args[i].string_value) {
                    result.string_value = strdup(args[i].string_value);
                } else {
                    result.int_value = args[i].int_value;
                }
                return result;
            }
        }
        return result;
    }
    
    // POWER(base, exponent)
    if (strcasecmp(func_name, "POWER") == 0 && arg_count >= 2) {
        double base = 0, exponent = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            base = (double)args[0].int_value;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            base = args[0].double_value;
        } else {
            return result;
        }
        
        if (args[1].type == VALUE_TYPE_INTEGER) {
            exponent = (double)args[1].int_value;
        } else if (args[1].type == VALUE_TYPE_DOUBLE) {
            exponent = args[1].double_value;
        } else {
            return result;
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = pow(base, exponent);
        return result;
    }
    
    // SQRT(number)
    if (strcasecmp(func_name, "SQRT") == 0) {
        double val = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            val = (double)args[0].int_value;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        if (val < 0) {
            return result; // null for negative numbers
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = sqrt(val);
        return result;
    }
    
    // CEIL(number)
    if (strcasecmp(func_name, "CEIL") == 0 || strcasecmp(func_name, "CEILING") == 0) {
        double val = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = args[0].int_value;
            return result;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = ceil(val);
        return result;
    }
    
    // FLOOR(number)
    if (strcasecmp(func_name, "FLOOR") == 0) {
        double val = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = args[0].int_value;
            return result;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = floor(val);
        return result;
    }
    
    // ROUND(number, [decimals])
    if (strcasecmp(func_name, "ROUND") == 0) {
        double val = 0;
        int decimals = 0;
        
        if (args[0].type == VALUE_TYPE_INTEGER) {
            val = (double)args[0].int_value;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        // optional second argument for decimal places
        if (arg_count >= 2) {
            if (args[1].type == VALUE_TYPE_INTEGER) {
                decimals = (int)args[1].int_value;
            } else if (args[1].type == VALUE_TYPE_DOUBLE) {
                decimals = (int)args[1].double_value;
            }
        }
        
        // round to specified decimal places
        double multiplier = pow(10.0, decimals);
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = round(val * multiplier) / multiplier;
        
        // if no decimals specified and result is whole number, return as integer
        if (decimals == 0 && result.double_value == floor(result.double_value)) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = (long long)result.double_value;
        }
        
        return result;
    }
    
    // ABS(number)
    if (strcasecmp(func_name, "ABS") == 0) {
        if (args[0].type == VALUE_TYPE_INTEGER) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = llabs(args[0].int_value);
            return result;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            result.type = VALUE_TYPE_DOUBLE;
            result.double_value = fabs(args[0].double_value);
            return result;
        }
        return result;
    }
    
    // EXP(number) e^x
    if (strcasecmp(func_name, "EXP") == 0) {
        double val = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            val = (double)args[0].int_value;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = exp(val);
        return result;
    }
    
    // LN(number) natural logarithm
    if (strcasecmp(func_name, "LN") == 0 || strcasecmp(func_name, "LOG") == 0) {
        double val = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            val = (double)args[0].int_value;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        if (val <= 0) {
            return result; // null for non-positive numbers
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = log(val);
        return result;
    }
    
    // MOD(dividend, divisor)
    if (strcasecmp(func_name, "MOD") == 0 && arg_count >= 2) {
        if (args[0].type == VALUE_TYPE_INTEGER && args[1].type == VALUE_TYPE_INTEGER) {
            if (args[1].int_value == 0) {
                return result; // null for division by zero
            }
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = args[0].int_value % args[1].int_value;
            return result;
        } else {
            double dividend = 0, divisor = 0;
            if (args[0].type == VALUE_TYPE_INTEGER) {
                dividend = (double)args[0].int_value;
            } else if (args[0].type == VALUE_TYPE_DOUBLE) {
                dividend = args[0].double_value;
            } else {
                return result;
            }
            
            if (args[1].type == VALUE_TYPE_INTEGER) {
                divisor = (double)args[1].int_value;
            } else if (args[1].type == VALUE_TYPE_DOUBLE) {
                divisor = args[1].double_value;
            } else {
                return result;
            }
            
            if (divisor == 0) {
                return result; // null for division by zero
            }
            
            result.type = VALUE_TYPE_DOUBLE;
            result.double_value = fmod(dividend, divisor);
            return result;
        }
    }
    
    // DATE parse date string
    if (strcasecmp(func_name, "DATE") == 0) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value) {
            DateValue date;
            if (parse_date(args[0].string_value, &date)) {
                result.type = VALUE_TYPE_DATE;
                result.date_value = date;
            }
        } else if (args[0].type == VALUE_TYPE_DATE) {
            // already a date, just return it
            result = args[0];
        }
        return result;
    }
    
    // CURRENT_DATE
    if (strcasecmp(func_name, "CURRENT_DATE") == 0) {
        result.type = VALUE_TYPE_DATE;
        result.date_value = current_date();
        return result;
    }
    
    // YEAR
    if (strcasecmp(func_name, "YEAR") == 0) {
        if (args[0].type == VALUE_TYPE_DATE) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = date_get_year(args[0].date_value);
        }
        return result;
    }
    
    // MONTH
    if (strcasecmp(func_name, "MONTH") == 0) {
        if (args[0].type == VALUE_TYPE_DATE) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = date_get_month(args[0].date_value);
        }
        return result;
    }
    
    // DAY
    if (strcasecmp(func_name, "DAY") == 0) {
        if (args[0].type == VALUE_TYPE_DATE) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = date_get_day(args[0].date_value);
        }
        return result;
    }
    
    // DAYOFWEEK
    if (strcasecmp(func_name, "DAYOFWEEK") == 0) {
        if (args[0].type == VALUE_TYPE_DATE) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = date_get_dayofweek(args[0].date_value);
        }
        return result;
    }
    
    // DAYOFYEAR
    if (strcasecmp(func_name, "DAYOFYEAR") == 0) {
        if (args[0].type == VALUE_TYPE_DATE) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = date_get_dayofyear(args[0].date_value);
        }
        return result;
    }
    
    // DATE_ADD(date, interval, unit)
    if (strcasecmp(func_name, "DATE_ADD") == 0 && arg_count >= 3) {
        if (args[0].type == VALUE_TYPE_DATE && 
            args[1].type == VALUE_TYPE_INTEGER &&
            args[2].type == VALUE_TYPE_STRING) {
            
            DateValue date = args[0].date_value;
            int interval = (int)args[1].int_value;
            const char* unit = args[2].string_value;
            
            if (strcasecmp(unit, "DAYS") == 0 || strcasecmp(unit, "DAY") == 0) {
                result.type = VALUE_TYPE_DATE;
                result.date_value = date_add_days(date, interval);
            } else if (strcasecmp(unit, "MONTHS") == 0 || strcasecmp(unit, "MONTH") == 0) {
                result.type = VALUE_TYPE_DATE;
                result.date_value = date_add_months(date, interval);
            } else if (strcasecmp(unit, "YEARS") == 0 || strcasecmp(unit, "YEAR") == 0) {
                result.type = VALUE_TYPE_DATE;
                result.date_value = date_add_years(date, interval);
            }
        }
        return result;
    }
    
    // DATE_SUB(date, interval, unit)
    if (strcasecmp(func_name, "DATE_SUB") == 0 && arg_count >= 3) {
        if (args[0].type == VALUE_TYPE_DATE && 
            args[1].type == VALUE_TYPE_INTEGER &&
            args[2].type == VALUE_TYPE_STRING) {
            
            DateValue date = args[0].date_value;
            int interval = -(int)args[1].int_value;  // negate for subtraction
            const char* unit = args[2].string_value;
            
            if (strcasecmp(unit, "DAYS") == 0 || strcasecmp(unit, "DAY") == 0) {
                result.type = VALUE_TYPE_DATE;
                result.date_value = date_add_days(date, interval);
            } else if (strcasecmp(unit, "MONTHS") == 0 || strcasecmp(unit, "MONTH") == 0) {
                result.type = VALUE_TYPE_DATE;
                result.date_value = date_add_months(date, interval);
            } else if (strcasecmp(unit, "YEARS") == 0 || strcasecmp(unit, "YEAR") == 0) {
                result.type = VALUE_TYPE_DATE;
                result.date_value = date_add_years(date, interval);
            }
        }
        return result;
    }
    
    // DATE_DIFF(date1, date2, unit)
    if (strcasecmp(func_name, "DATE_DIFF") == 0 && arg_count >= 3) {
        if (args[0].type == VALUE_TYPE_DATE && 
            args[1].type == VALUE_TYPE_DATE &&
            args[2].type == VALUE_TYPE_STRING) {
            
            DateValue date1 = args[0].date_value;
            DateValue date2 = args[1].date_value;
            const char* unit = args[2].string_value;
            
            result.type = VALUE_TYPE_INTEGER;
            if (strcasecmp(unit, "DAYS") == 0 || strcasecmp(unit, "DAY") == 0) {
                result.int_value = date_diff_days(date1, date2);
            } else if (strcasecmp(unit, "MONTHS") == 0 || strcasecmp(unit, "MONTH") == 0) {
                result.int_value = date_diff_months(date1, date2);
            } else if (strcasecmp(unit, "YEARS") == 0 || strcasecmp(unit, "YEAR") == 0) {
                result.int_value = date_diff_years(date1, date2);
            }
        }
        return result;
    }
    
    // DATE_FORMAT(date, format_string)
    if (strcasecmp(func_name, "DATE_FORMAT") == 0 && arg_count >= 2) {
        if (args[0].type == VALUE_TYPE_DATE && 
            args[1].type == VALUE_TYPE_STRING && args[1].string_value) {
            
            DateFormat format = DATE_FORMAT_ISO;
            const char* fmt = args[1].string_value;
            
            if (strcasecmp(fmt, "ISO") == 0 || strcasecmp(fmt, "YYYY-MM-DD") == 0) {
                format = DATE_FORMAT_ISO;
            } else if (strcasecmp(fmt, "US") == 0 || strcasecmp(fmt, "MM/DD/YYYY") == 0) {
                format = DATE_FORMAT_US;
            } else if (strcasecmp(fmt, "EU") == 0 || strcasecmp(fmt, "DD/MM/YYYY") == 0) {
                format = DATE_FORMAT_EU;
            } else if (strcasecmp(fmt, "COMPACT") == 0 || strcasecmp(fmt, "YYYYMMDD") == 0) {
                format = DATE_FORMAT_COMPACT;
            }
            
            result.type = VALUE_TYPE_STRING;
            result.string_value = format_date(args[0].date_value, format);
        }
        return result;
    }
    
    return result;
}
