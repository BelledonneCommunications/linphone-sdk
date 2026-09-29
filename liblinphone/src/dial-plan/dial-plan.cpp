/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstring>
#include <functional>

#include "linphone/utils/utils.h"

#include "dial-plan.h"
#include "logger/logger.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

/*
 * http://en.wikipedia.org/wiki/Telephone_numbering_plan
 * http://en.wikipedia.org/wiki/Telephone_numbers_in_Europe
 * imported from https://en.wikipedia.org/wiki/List_of_mobile_phone_number_series_by_country
 * https://www.itu.int/dms_pub/itu-t/opb/sp/T-SP-E.164C-2011-PDF-E.pdf (2011)
 */
const list<shared_ptr<DialPlan>> DialPlan::sDialPlans = {
    // Country, iso country code, e164 country calling code, minimum phone numbering plan length, maximum phone
    // numbering plan length, international usual prefix, flag, trunk prefix
    DialPlan::create("Afghanistan", "AF", "93", 9, 9, "00", "🇦🇫", "0"),
    DialPlan::create("Albania", "AL", "355", 3, 9, "00", "🇦🇱", "0"),
    DialPlan::create("Algeria", "DZ", "213", 8, 9, "00", "🇩🇿", "0"),
    DialPlan::create("American Samoa", "AS", "1", 10, 10, "011", "🇦🇸", "1"),
    DialPlan::create("Andorra", "AD", "376", 6, 6, "00", "🇦🇩", ""),
    DialPlan::create("Angola", "AO", "244", 9, 9, "00", "🇦🇴", ""),
    DialPlan::create("Anguilla", "AI", "1", 10, 10, "011", "🇦🇮", "1"),
    DialPlan::create("Antigua and Barbuda", "AG", "1", 10, 10, "011", "🇦🇬", "1"),
    DialPlan::create("Argentina", "AR", "54", 10, 10, "00", "🇦🇷", "0"),
    DialPlan::create("Armenia", "AM", "374", 8, 8, "00", "🇦🇲", "0"),
    DialPlan::create("Aruba", "AW", "297", 7, 7, "011", "🇦🇼", ""),
    DialPlan::create("Australia", "AU", "61", 9, 9, "0011", "🇦🇺", "0"),
    DialPlan::create("Austria", "AT", "43", 4, 13, "00", "🇦🇹", "0"),
    DialPlan::create("Azerbaijan", "AZ", "994", 8, 9, "00", "🇦🇿", "0"),
    DialPlan::create("Bahamas", "BS", "1", 10, 10, "011", "🇧🇸", "1"),
    DialPlan::create("Bahrain", "BH", "973", 8, 8, "00", "🇧🇭", ""),
    DialPlan::create("Bangladesh", "BD", "880", 6, 10, "00", "🇧🇩", "0"),
    DialPlan::create("Barbados", "BB", "1", 10, 10, "011", "🇧🇧", "1"),
    DialPlan::create("Belarus", "BY", "375", 9, 9, "00", "🇧🇾", "80"),
    DialPlan::create("Belgium", "BE", "32", 8, 9, "00", "🇧🇪", "0"),
    DialPlan::create("Belize", "BZ", "501", 7, 7, "00", "🇧🇿", ""),
    DialPlan::create("Benin", "BJ", "229", 10, 10, "00", "🇧🇯", ""),
    DialPlan::create("Bermuda", "BM", "1", 10, 10, "011", "🇧🇲", "1"),
    DialPlan::create("Bhutan", "BT", "975", 7, 8, "00", "🇧🇹", ""),
    DialPlan::create("Bolivia", "BO", "591", 8, 8, "00", "🇧🇴", "0"),
    DialPlan::create("Bosnia and Herzegovina", "BA", "387", 8, 8, "00", "🇧🇦", "0"),
    DialPlan::create("Botswana", "BW", "267", 7, 8, "00", "🇧🇼", ""),
    DialPlan::create("Brazil", "BR", "55", 8, 11, "00", "🇧🇷", "0"),
    DialPlan::create("Brunei Darussalam", "BN", "673", 7, 7, "00", "🇧🇳", ""),
    DialPlan::create("Bulgaria", "BG", "359", 7, 9, "00", "🇧🇬", "0"),
    DialPlan::create("Burkina Faso", "BF", "226", 8, 8, "00", "🇧🇫", ""),
    DialPlan::create("Burundi", "BI", "257", 8, 8, "011", "🇧🇮", ""),
    DialPlan::create("Cambodia", "KH", "855", 8, 9, "00", "🇰🇭", "0"),
    DialPlan::create("Cameroon", "CM", "237", 8, 9, "00", "🇨🇲", ""),
    DialPlan::create("Canada", "CA", "1", 10, 10, "011", "🇨🇦", "1"),
    DialPlan::create("Cape Verde", "CV", "238", 7, 7, "00", "🇨🇻", ""),
    DialPlan::create("Cayman Islands", "KY", "1", 10, 10, "011", "🇰🇾", "1"),
    DialPlan::create("Central African Republic", "CF", "236", 8, 8, "00", "🇨🇫", ""),
    DialPlan::create("Chad", "TD", "235", 8, 8, "00", "🇹🇩", ""),
    DialPlan::create("Chile", "CL", "56", 8, 9, "00", "🇨🇱", ""),
    DialPlan::create("China", "CN", "86", 7, 11, "00", "🇨🇳", "0"),
    DialPlan::create("Colombia", "CO", "57", 8, 10, "00", "🇨🇴", ""),
    DialPlan::create("Comoros", "KM", "269", 7, 7, "00", "🇰🇲", ""),
    DialPlan::create("Cook Islands", "CK", "682", 5, 5, "00", "🇨🇰", ""),
    DialPlan::create("Costa Rica", "CR", "506", 8, 8, "00", "🇨🇷", ""),
    DialPlan::create("Cote d'Ivoire", "AD", "225", 8, 10, "00", "🇨🇮", ""),
    DialPlan::create("Croatia", "HR", "385", 7, 9, "00", "🇭🇷", "0"),
    DialPlan::create("Cuba", "CU", "53", 6, 8, "119", "🇨🇺", "0"),
    DialPlan::create("Curacao", "CUR", "599", 7, 8, "011", "🇨🇼", "0"),
    DialPlan::create("Cyprus", "CY", "357", 8, 8, "00", "🇨🇾", ""),
    DialPlan::create("Czech Republic", "CZ", "420", 9, 9, "00", "🇨🇿", ""),
    DialPlan::create("Democratic Republic of the Congo", "CD", "243", 5, 9, "00", "🇨🇩", "0"),
    DialPlan::create("Denmark", "DK", "45", 8, 8, "00", "🇩🇰", ""),
    DialPlan::create("Djibouti", "DJ", "253", 8, 8, "00", "🇩🇯", ""),
    DialPlan::create("Dominica", "DM", "1", 10, 10, "011", "🇩🇲", "1"),
    DialPlan::create("Dominican Republic", "DO", "1", 10, 10, "011", "🇩🇴", "1"),
    DialPlan::create("East Timor", "TLS", "670", 7, 8, "00", "🇹🇱", ""),
    DialPlan::create("Ecuador", "EC", "593", 8, 9, "00", "🇪🇨", "0"),
    DialPlan::create("Egypt", "EG", "20", 8, 10, "00", "🇪🇬", "0"),
    DialPlan::create("El Salvador", "SV", "503", 7, 8, "00", "🇸🇻", ""),
    DialPlan::create("Equatorial Guinea", "GQ", "240", 9, 9, "00", "🇬🇶", ""),
    DialPlan::create("Eritrea", "ER", "291", 7, 7, "00", "🇪🇷", "0"),
    DialPlan::create("Estonia", "EE", "372", 7, 8, "00", "🇪🇪", ""),
    DialPlan::create("Eswatini", "SZ", "268", 7, 8, "00", "🇸🇿", ""),
    DialPlan::create("Ethiopia", "ET", "251", 9, 9, "00", "🇪🇹", "0"),
    DialPlan::create("Falkland Islands", "FK", "500", 5, 5, "00", "🇫🇰", ""),
    DialPlan::create("Faroe Islands", "FO", "298", 6, 6, "00", "🇫🇴", ""),
    DialPlan::create("Fiji", "FJ", "679", 7, 7, "00", "🇫🇯", ""),
    DialPlan::create("Finland", "FI", "358", 5, 12, "00", "🇫🇮", "0"),
    DialPlan::create("France", "FR", "33", 9, 9, "00", "🇫🇷", "0"),
    DialPlan::create("French Guiana", "GF", "594", 9, 9, "00", "🇬🇫", "0"),
    DialPlan::create("French Polynesia", "PF", "689", 6, 6, "00", "🇵🇫", ""),
    DialPlan::create("Gabon", "GA", "241", 7, 8, "00", "🇬🇦", "0"),
    DialPlan::create("Gambia", "GM", "220", 7, 7, "00", "🇬🇲", ""),
    DialPlan::create("Georgia", "GE", "995", 9, 9, "00", "🇬🇪", "0"),
    DialPlan::create("Germany", "DE", "49", 6, 13, "00", "🇩🇪", "0"),
    DialPlan::create("Ghana", "GH", "233", 5, 9, "00", "🇬🇭", "0"),
    DialPlan::create("Gibraltar", "GI", "350", 8, 8, "00", "🇬🇮", ""),
    DialPlan::create("Greece", "GR", "30", 10, 10, "00", "🇬🇷", ""),
    DialPlan::create("Greenland", "GL", "299", 6, 6, "00", "🇬🇱", ""),
    DialPlan::create("Grenada", "GD", "1", 10, 10, "011", "🇬🇩", "1"),
    DialPlan::create("Guadeloupe", "GP", "590", 9, 9, "00", "🇬🇵", "0"),
    DialPlan::create("Guam", "GU", "1", 10, 10, "011", "🇬🇺", "1"),
    DialPlan::create("Guatemala", "GT", "502", 8, 8, "00", "🇬🇹", ""),
    DialPlan::create("Guinea", "GN", "224", 8, 8, "00", "🇬🇳", ""),
    DialPlan::create("Guinea-Bissau", "GW", "245", 7, 7, "00", "🇬🇼", ""),
    DialPlan::create("Guyana", "GY", "592", 7, 7, "001", "🇬🇾", ""),
    DialPlan::create("Haiti", "HT", "509", 8, 8, "00", "🇭🇹", ""),
    DialPlan::create("Honduras", "HN", "504", 8, 8, "00", "🇭🇳", ""),
    DialPlan::create("Hong Kong", "HK", "852", 8, 8, "001", "🇭🇰", ""),
    DialPlan::create("Hungary", "HU", "36", 8, 9, "00", "🇭🇺", "06"),
    DialPlan::create("Iceland", "IS", "354", 7, 7, "00", "🇮🇸", ""),
    DialPlan::create("India", "IN", "91", 7, 10, "00", "🇮🇳", "0"),
    DialPlan::create("Indonesia", "ID", "62", 7, 12, "001", "🇮🇩", "0"),
    DialPlan::create("Iran", "IR", "98", 6, 10, "00", "🇮🇷", "0"),
    DialPlan::create("Iraq", "IQ", "964", 8, 10, "00", "🇮🇶", ""),
    DialPlan::create("Ireland", "IE", "353", 7, 9, "00", "🇮🇪", "0"),
    DialPlan::create("Israel", "IL", "972", 8, 9, "00", "🇮🇱", "0"),
    DialPlan::create("Italy", "IT", "39", 6, 11, "00", "🇮🇹", ""),
    DialPlan::create("Jamaica", "JM", "1", 10, 10, "011", "🇯🇲", "1"),
    DialPlan::create("Japan", "JP", "81", 8, 10, "010", "🇯🇵", "0"),
    DialPlan::create("Jordan", "JO", "962", 5, 9, "00", "🇯🇴", "0"),
    DialPlan::create("Kenya", "KE", "254", 6, 9, "000", "🇰🇪", "0"),
    DialPlan::create("Kiribati", "KI", "686", 5, 5, "00", "🇰🇮", ""),
    DialPlan::create("Korea, North", "KP", "850", 8, 12, "99", "🇰🇵", ""),
    DialPlan::create("Korea, South", "KR", "82", 8, 12, "001", "🇰🇷", ""),
    DialPlan::create("Kosovo", "KOS", "383", 8, 8, "00", "🇽🇰", "0"),
    DialPlan::create("Kuwait", "KW", "965", 7, 8, "00", "🇰🇼", ""),
    DialPlan::create("Kyrgyzstan", "KG", "996", 9, 9, "00", "🇰🇬", "0"),
    DialPlan::create("Laos", "LA", "856", 8, 10, "00", "🇱🇦", "0"),
    DialPlan::create("Latvia", "LV", "371", 7, 8, "00", "🇱🇻", ""),
    DialPlan::create("Lebanon", "LB", "961", 7, 8, "00", "🇱🇧", "0"),
    DialPlan::create("Lesotho", "LS", "266", 8, 8, "00", "🇱🇸", ""),
    DialPlan::create("Liberia", "LR", "231", 7, 8, "00", "🇱🇷", ""),
    DialPlan::create("Libya", "LY", "218", 8, 10, "00", "🇱🇾", "0"),
    DialPlan::create("Liechtenstein", "LI", "423", 7, 7, "00", "🇱🇮", ""),
    DialPlan::create("Lithuania", "LT", "370", 8, 8, "00", "🇱🇹", "8"),
    DialPlan::create("Luxembourg", "LU", "352", 4, 9, "00", "🇱🇺", ""),
    DialPlan::create("Macau", "MO", "853", 7, 8, "00", "🇲🇴", ""),
    DialPlan::create("Madagascar", "MG", "261", 9, 9, "00", "🇲🇬", "0"),
    DialPlan::create("Malawi", "MW", "265", 7, 9, "00", "🇲🇼", ""),
    DialPlan::create("Malaysia", "MY", "60", 7, 10, "00", "🇲🇾", "0"),
    DialPlan::create("Maldives", "MV", "960", 7, 7, "00", "🇲🇻", ""),
    DialPlan::create("Mali", "ML", "223", 8, 8, "00", "🇲🇱", ""),
    DialPlan::create("Malta", "MT", "356", 8, 8, "00", "🇲🇹", ""),
    DialPlan::create("Marshall Islands", "MH", "692", 7, 7, "011", "🇲🇭", "1"),
    DialPlan::create("Martinique", "MQ", "596", 9, 9, "00", "🇲🇶", "0"),
    DialPlan::create("Mauritania", "MR", "222", 8, 8, "00", "🇲🇷", ""),
    DialPlan::create("Mauritius", "MU", "230", 7, 7, "00", "🇲🇺", ""),
    DialPlan::create("Mexico", "MX", "52", 10, 11, "00", "🇲🇽", ""),
    DialPlan::create("Micronesia", "FM", "691", 7, 7, "011", "🇫🇲", "1"),
    DialPlan::create("Moldova", "MD", "373", 8, 8, "00", "🇲🇩", "0"),
    DialPlan::create("Monaco", "MC", "377", 8, 8, "00", "🇲🇨", ""),
    DialPlan::create("Mongolia", "MN", "976", 7, 8, "001", "🇲🇳", "0"),
    DialPlan::create("Montenegro", "ME", "382", 8, 8, "00", "🇲🇪", "0"),
    DialPlan::create("Montserrat", "MS", "1", 10, 10, "011", "🇲🇸", "1"),
    DialPlan::create("Morocco", "MA", "212", 9, 9, "00", "🇲🇦", "0"),
    DialPlan::create("Mozambique", "MZ", "258", 8, 9, "00", "🇲🇿", ""),
    DialPlan::create("Myanmar", "MM", "95", 7, 10, "00", "🇲🇲", "0"),
    DialPlan::create("Namibia", "NA", "264", 8, 9, "00", "🇳🇦", "0"),
    DialPlan::create("Nauru", "NR", "674", 4, 7, "00", "🇳🇷", ""),
    DialPlan::create("Nepal", "NP", "977", 8, 8, "00", "🇳🇵", "0"),
    DialPlan::create("Netherlands", "NL", "31", 9, 9, "00", "🇳🇱", "0"),
    DialPlan::create("New Caledonia", "NC", "687", 6, 6, "00", "🇳🇨", ""),
    // If number starts by 210 then it may have 10 digits...
    DialPlan::create("New Zealand", "NZ", "64", 3, 10, "00", "🇳🇿", "0"),
    DialPlan::create("Nicaragua", "NI", "505", 8, 8, "00", "🇳🇮", ""),
    DialPlan::create("Niger", "NE", "227", 8, 8, "00", "🇳🇪", ""),
    DialPlan::create("Nigeria", "NG", "234", 7, 10, "009", "🇳🇬", "0"),
    DialPlan::create("Niue", "NU", "683", 4, 4, "00", "🇳🇺", ""),
    DialPlan::create("Norfolk Island", "NF", "672", 6, 6, "00", "🇳🇫", ""),
    DialPlan::create("North Macedonia", "MK", "389", 8, 8, "00", "🇲🇰", "0"),
    DialPlan::create("Northern Mariana Islands", "MP", "1", 10, 10, "011", "🇲🇵", "1"),
    DialPlan::create("Norway", "NO", "47", 5, 8, "00", "🇳🇴", ""),
    DialPlan::create("Oman", "OM", "968", 7, 8, "00", "🇴🇲", ""),
    DialPlan::create("Pakistan", "PK", "92", 6, 10, "00", "🇵🇰", "0"),
    DialPlan::create("Palau", "PW", "680", 7, 7, "011", "🇵🇼", ""),
    DialPlan::create("Palestine", "PS", "970", 9, 9, "00", "🇵🇸", "0"),
    DialPlan::create("Panama", "PA", "507", 7, 8, "00", "🇵🇦", ""),
    DialPlan::create("Papua New Guinea", "PG", "675", 7, 8, "00", "🇵🇬", ""),
    DialPlan::create("Paraguay", "PY", "595", 5, 9, "00", "🇵🇾", "0"),
    DialPlan::create("Peru", "PE", "51", 8, 9, "00", "🇵🇪", "0"),
    DialPlan::create("Philippines", "PH", "63", 8, 10, "00", "🇵🇭", "0"),
    DialPlan::create("Poland", "PL", "48", 6, 9, "00", "🇵🇱", ""),
    DialPlan::create("Portugal", "PT", "351", 9, 9, "00", "🇵🇹", ""),
    DialPlan::create("Puerto Rico", "PR", "1", 10, 10, "011", "🇵🇷", "1"),
    DialPlan::create("Qatar", "QA", "974", 3, 8, "00", "🇶🇦", ""),
    DialPlan::create("Republic of the Congo", "CG", "242", 9, 9, "00", "🇨🇬", ""),
    DialPlan::create("Reunion Island", "RE", "262", 9, 9, "011", "🇷🇪", "0"),
    DialPlan::create("Romania", "RO", "40", 9, 9, "00", "🇷🇴", "0"),
    DialPlan::create("Russian Federation", "RU", "7", 10, 10, "8", "🇷🇺", "8"),
    DialPlan::create("Rwanda", "RW", "250", 9, 9, "00", "🇷🇼", ""),
    DialPlan::create("Saint Helena", "SH", "290", 4, 4, "00", "🇸🇭", ""),
    DialPlan::create("Saint Kitts and Nevis", "KN", "1", 10, 10, "011", "🇰🇳", "1"),
    DialPlan::create("Saint Lucia", "LC", "1", 10, 10, "011", "🇱🇨", "1"),
    DialPlan::create("Saint Pierre and Miquelon", "PM", "508", 6, 6, "00", "🇵🇲", ""),
    DialPlan::create("Saint Vincent and the Grenadines", "VC", "1", 10, 10, "011", "🇻🇨", "1"),
    DialPlan::create("Samoa", "WS", "685", 3, 7, "0", "🇼🇸", ""),
    DialPlan::create("San Marino", "SM", "378", 6, 10, "00", "🇸🇲", ""),
    DialPlan::create("Sao Tome and Principe", "ST", "239", 7, 7, "00", "🇸🇹", ""),
    DialPlan::create("Saudi Arabia", "SA", "966", 8, 9, "00", "🇸🇦", "0"),
    DialPlan::create("Senegal", "SN", "221", 9, 9, "00", "🇸🇳", ""),
    DialPlan::create("Serbia", "RS", "381", 6, 9, "00", "🇷🇸", "0"),
    DialPlan::create("Seychelles", "SC", "248", 7, 7, "00", "🇸🇨", ""),
    DialPlan::create("Sierra Leone", "SL", "232", 8, 8, "00", "🇸🇱", "0"),
    DialPlan::create("Singapore", "SG", "65", 8, 8, "001", "🇸🇬", ""),
    DialPlan::create("Slovakia", "SK", "421", 4, 9, "00", "🇸🇰", "0"),
    DialPlan::create("Slovenia", "SI", "386", 8, 8, "00", "🇸🇮", "0"),
    DialPlan::create("Solomon Islands", "SB", "677", 5, 7, "00", "🇸🇧", ""),
    DialPlan::create("Somalia", "SO", "252", 5, 8, "00", "🇸🇴", "0"),
    DialPlan::create("South Africa", "ZA", "27", 9, 9, "00", "🇿🇦", "0"),
    DialPlan::create("South Sudan", "SSD", "211", 9, 9, "00", "🇸🇸", "0"),
    DialPlan::create("Spain", "ES", "34", 9, 9, "00", "🇪🇸", ""),
    DialPlan::create("Sri Lanka", "LK", "94", 9, 9, "00", "🇱🇰", "0"),
    DialPlan::create("Sudan", "SD", "249", 9, 9, "00", "🇸🇩", "0"),
    DialPlan::create("Suriname", "SR", "597", 6, 7, "00", "🇸🇷", "0"),
    DialPlan::create("Sweden", "SE", "46", 7, 9, "00", "🇸🇪", "0"),
    DialPlan::create("Switzerland", "XK", "41", 9, 9, "00", "🇨🇭", "0"),
    DialPlan::create("Syria", "SY", "963", 8, 9, "00", "🇸🇾", "0"),
    DialPlan::create("Taiwan", "TW", "886", 8, 9, "810", "🇹🇼", "0"),
    DialPlan::create("Tajikistan", "TJ", "992", 9, 9, "002", "🇹🇯", "8"),
    DialPlan::create("Tanzania", "TZ", "255", 9, 9, "000", "🇹🇿", "0"),
    DialPlan::create("Thailand", "TH", "66", 8, 9, "001", "🇹🇭", "0"),
    DialPlan::create("Togo", "TG", "228", 8, 8, "00", "🇹🇬", ""),
    DialPlan::create("Tokelau", "TK", "690", 4, 4, "00", "🇹🇰", ""),
    DialPlan::create("Tonga", "TO", "676", 5, 7, "00", "🇹🇴", ""),
    DialPlan::create("Trinidad and Tobago", "TT", "1", 10, 10, "011", "🇹🇹", "1"),
    DialPlan::create("Tunisia", "TN", "216", 8, 8, "00", "🇹🇳", ""),
    DialPlan::create("Turkey", "TR", "90", 10, 10, "00", "🇹🇷", "0"),
    DialPlan::create("Turkmenistan", "TM", "993", 8, 8, "00", "🇹🇲", "8"),
    DialPlan::create("Turks and Caicos Islands", "TC", "1", 7, 7, "0", "🇹🇨", "1"),
    DialPlan::create("Tuvalu", "TV", "688", 5, 5, "00", "🇹🇻", ""),
    DialPlan::create("Uganda", "UG", "256", 9, 9, "000", "🇺🇬", "0"),
    DialPlan::create("Ukraine", "UA", "380", 9, 9, "00", "🇺🇦", "0"),
    DialPlan::create("United Arab Emirates", "AE", "971", 8, 9, "00", "🇦🇪", "0"),
    DialPlan::create("United Kingdom", "GB", "44", 7, 10, "00", "🇬🇧", "0"),
    DialPlan::create("United States", "US", "1", 10, 10, "011", "🇺🇸", "1"),
    DialPlan::create("Uruguay", "UY", "598", 8, 8, "00", "🇺🇾", "0"),
    DialPlan::create("Uzbekistan", "UZ", "998", 9, 9, "8", "🇺🇿", "0"),
    DialPlan::create("Vanuatu", "VU", "678", 5, 7, "00", "🇻🇺", ""),
    DialPlan::create("Venezuela", "VE", "58", 10, 10, "00", "🇻🇪", "0"),
    DialPlan::create("Vietnam", "VN", "84", 7, 10, "00", "🇻🇳", "0"),
    DialPlan::create("Wallis and Futuna", "WF", "681", 6, 6, "00", "🇼🇫", ""),
    DialPlan::create("Yemen", "YE", "967", 6, 9, "00", "🇾🇪", "0"),
    DialPlan::create("Zambia", "ZM", "260", 9, 9, "00", "🇿🇲", "0"),
    DialPlan::create("Zimbabwe", "ZW", "263", 9, 9, "00", "🇿🇼", "0")};

const shared_ptr<DialPlan> DialPlan::MostCommon = DialPlan::create("generic", "", "", 0, 10, "00", "");

DialPlan::DialPlan(const string &country,
                   const string &isoCountryCode,
                   const string &ccc,
                   int minNnl,
                   int maxNnl,
                   const string &icp,
                   const string &flagEmoji,
                   const string &trunkPrefix,
                   const std::optional<std::function<size_t(const std::string)>> &nationalNumberLengthFunction)
    : country(country), isoCountryCode(isoCountryCode), countryCallingCode(ccc), minNationalNumberLength(minNnl),
      maxNationalNumberLength(maxNnl), internationalCallPrefix(icp), flag(flagEmoji), mTrunkPrefix(trunkPrefix) {
	if (nationalNumberLengthFunction.has_value()) {
		mNationalNumberLengthFunction = *nationalNumberLengthFunction;
	} else {
		mNationalNumberLengthFunction = nullptr;
	}
}

DialPlan &DialPlan::operator=(const DialPlan &other) {
	if (this != &other) {
		country = other.getCountry();
		isoCountryCode = other.getIsoCountryCode();
		countryCallingCode = other.getCountryCallingCode();
		minNationalNumberLength = other.getMinNationalNumberLength();
		maxNationalNumberLength = other.getMaxNationalNumberLength();
		internationalCallPrefix = other.getInternationalCallPrefix();
		flag = other.getFlag();
		if (other.mNationalNumberLengthFunction) {
			mNationalNumberLengthFunction = other.mNationalNumberLengthFunction;
		}
		mTrunkPrefix = other.mTrunkPrefix;
	}

	return *this;
}

const string &DialPlan::getCountry() const {
	return country;
}

const string &DialPlan::getIsoCountryCode() const {
	return isoCountryCode;
}

const string &DialPlan::getCountryCallingCode() const {
	return countryCallingCode;
}

void DialPlan::setCountryCallingCode(const std::string &ccc) {
	countryCallingCode = ccc;
}

int DialPlan::getMinNationalNumberLength() const {
	return minNationalNumberLength;
}

int DialPlan::getMaxNationalNumberLength() const {
	return maxNationalNumberLength;
}

const string &DialPlan::getInternationalCallPrefix() const {
	return internationalCallPrefix;
}

const string &DialPlan::getFlag() const {
	return flag;
}

const string &DialPlan::getTrunkPrefix() const {
	return mTrunkPrefix;
}

bool DialPlan::isGeneric() const {
	return country == MostCommon->getCountry();
}

int DialPlan::lookupCccFromE164(const string &e164) {
	if (e164[0] != '+') return -1; // Not an e164 number.

	// USA case.
	if (e164[1] == '1') return 1;

	shared_ptr<DialPlan> electedDialPlan;
	unsigned int found;
	unsigned int i = 0;

	do {
		found = 0;
		i++;
		for (const auto &dp : sDialPlans) {
			if (strncmp(dp->getCountryCallingCode().c_str(), &e164[1], i) == 0) {
				electedDialPlan = dp;
				found++;
			}
		}
	} while ((found > 1 || found == 0) && i < e164.length() - 1);

	if (found == 1) return Utils::stoi(electedDialPlan->getCountryCallingCode());

	return -1;
}

int DialPlan::lookupCccFromIso(const string &iso) {
	for (const auto &dp : sDialPlans) {
		if (dp->getIsoCountryCode() == iso) return Utils::stoi(dp->getCountryCallingCode());
	}

	return -1;
}

shared_ptr<DialPlan> DialPlan::findByCcc(int ccc) {
	return DialPlan::findByCcc(Utils::toString(ccc));
}

shared_ptr<DialPlan> DialPlan::findByCcc(const string &ccc) {
	if (ccc.empty()) return MostCommon;

	for (const auto &dp : sDialPlans) {
		if (dp->getCountryCallingCode() == ccc) return dp;
	}

	// Return a generic "most common" dial plan.
	return MostCommon;
}

string DialPlan::flattenPhoneNumber(const string &number) const {
	char *unescaped_phone_number = belle_sip_username_unescape_unnecessary_characters(number.c_str());
	char *result = reinterpret_cast<char *>(ms_malloc0(strlen(unescaped_phone_number) + 1));
	char *w = result;
	const char *r;

	for (r = unescaped_phone_number; *r != '\0'; ++r) {
		if (*r == '+' || isdigit(*r)) {
			*w++ = *r;
		}
	}

	*w++ = '\0';
	belle_sip_free(unescaped_phone_number);
	string cppResult = string(result);
	ms_free(result);
	return cppResult;
}

string DialPlan::getSignificantDigits(const string &phoneNumber) const {
	string flatten = flattenPhoneNumber(phoneNumber);
	lDebug() << "[DialPlan] Flattened number for [" << phoneNumber << "] is: [" << flatten << "]";

	// if flatten starts by +, remove it
	if (!flatten.empty() && flatten[0] == '+') {
		flatten = flatten.substr(1);
		lDebug() << "[DialPlan] Flattened number started by +, removing it: [" << flatten << "]";

		size_t countryCallingCodePos = flatten.find(countryCallingCode);
		// if flatten starts with country calling code (but isn't equal to it!), remove it first
		if (countryCallingCodePos == 0 && flatten.length() != countryCallingCode.length()) {
			flatten = flatten.substr(countryCallingCodePos + countryCallingCode.length());
			lDebug() << "[DialPlan] Flattened number started by [" << countryCallingCode << "], removing it: ["
			         << flatten << "]";
		}
	}
	return flatten;
}

string DialPlan::formatPhoneNumber(const string &phoneNumber, bool escapePlus) const {
	if (!countryCallingCode.empty()) {
		string flatten = getSignificantDigits(phoneNumber);

		// Keep at most national number significant digits
		size_t length = flatten.length();
		size_t nationalNumberLength = mNationalNumberLengthFunction
		                                  ? MIN((size_t)maxNationalNumberLength, mNationalNumberLengthFunction(flatten))
		                                  : (size_t)maxNationalNumberLength;
		lDebug() << "[DialPlan] nationalNumberLength = " << nationalNumberLength;
		if (length > nationalNumberLength) {
			flatten = flatten.substr(length - (size_t)nationalNumberLength, length);
			lDebug() << "[DialPlan] Keeping the last [" << nationalNumberLength << "] digits: [" << flatten << "]";
		} else if (length < (size_t)minNationalNumberLength) {
			lDebug() << "[DialPlan] Flattened number has not enough significant digits";
			lDebug() << "[DialPlan] Result is [" << phoneNumber << "]";
			return phoneNumber;
		}

		if (!flatten.empty() && !mTrunkPrefix.empty() && length >= (size_t)minNationalNumberLength &&
		    flatten.rfind(mTrunkPrefix, 0) == 0) {
			// Remove first character(s) if it matches country trunk prefix when applying international prefix to phone
			// number (not to shortcodes)
			flatten = flatten.substr(mTrunkPrefix.length());
			lDebug() << "[DialPlan] Flattened number started by country trunk prefix [" << mTrunkPrefix
			         << "], removing it: [" << flatten << "]";
		}

		// Prepend international calling prefix or +
		string prefix = escapePlus ? internationalCallPrefix : "+";
		// Add country calling code followed by phone number digits
		string result = string(prefix + countryCallingCode + flatten);
		lDebug() << "[DialPlan] Result is [" << result << "]";

		return result;
	}

	return phoneNumber;
}

const list<shared_ptr<DialPlan>> &DialPlan::getAllDialPlans() {
	return sDialPlans;
}

bool DialPlan::isShortNumber(const std::string &phoneNumber) {
	if (phoneNumber[0] != '+' && phoneNumber.length() < (size_t)getMinNationalNumberLength()) {
		lDebug() << "[DialPlan] Flattened number [" << phoneNumber << "] is a short number in dial plan ["
		         << getCountry() << "]";
		return true;
	}
	return false;
}

bool DialPlan::isShortNumber(const int ccc, const std::string &phoneNumber) {
	std::shared_ptr<DialPlan> dialPlan = findByCcc(ccc);
	return dialPlan->isShortNumber(phoneNumber);
}

bool DialPlan::hasEnoughSignificantDigits(const int ccc, const std::string &phoneNumber) {
	std::shared_ptr<DialPlan> dialPlan = findByCcc(ccc);

	string flatten = dialPlan->getSignificantDigits(phoneNumber);

	if (flatten.length() < (size_t)dialPlan->getMinNationalNumberLength()) {
		lDebug() << "[DialPlan] Flattened number [" << phoneNumber << "] has not enough digits in dial plan ["
		         << dialPlan->getCountry() << "]";
		return FALSE;
	}
	return TRUE;
}

LINPHONE_END_NAMESPACE
