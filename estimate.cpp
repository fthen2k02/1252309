/*
 * Copyright (C) 2024 The Infinity Chiller
 */

#include <iostream>
#include <fstream>
#include <random>
#include <array>
#include <vector>
#include <chrono>
#include <iomanip>

#define NUM_LETTERS             26
#define MSG_LENGTH_PLAIN        19
#define MSG_LENGTH_A1Z26        (MSG_LENGTH_PLAIN * 2)
#define MIN_TIMESTAMP_LENGTH    (2 + 1 + 1 + 1)

#define NUM_TIMESTAMP_FIELDS    4
#define POSSIBLE_FIELD_ORDERS   3

using namespace std;

class Message
{
private:
    array<char, MSG_LENGTH_A1Z26> a1z26;

public:
    void generate();
    bool getInt(int pos, int size, int& val) const;

    friend ostream& operator<<(ostream& g, const Message& m);
} m;

class IncrementallyCheckedTimestamp
{
private:
    array<int, NUM_TIMESTAMP_FIELDS> fields;
    array<bool, NUM_TIMESTAMP_FIELDS> known;
public:
    enum FieldType { FieldYear, FieldMonth, FieldDay, FieldHour, FieldCount };
    enum FieldSizeType { SizeShort, SizeLong, SizeCount };

    const int orders[POSSIBLE_FIELD_ORDERS][NUM_TIMESTAMP_FIELDS] = {
        { FieldDay, FieldMonth, FieldYear, FieldHour },
        { FieldMonth, FieldDay, FieldYear, FieldHour },
        { FieldYear, FieldMonth, FieldDay, FieldHour }
    };

    bool setField(int pos, int val);
    void unsetField(int pos);
    void processTimestamps(const Message& m, int msgPos, int order, int fieldIndex);

    static bool checkShortMonth(int day, int month);
    static bool checkLeap(int day, int month, int year);
    static int getFieldSize(int fieldType, int fieldSizeType);

    friend istream& operator>>(istream& f, IncrementallyCheckedTimestamp& m);
    friend bool operator<=(const IncrementallyCheckedTimestamp& t1, const IncrementallyCheckedTimestamp& t2);
} currentTimestamp;

mt19937 gen;
discrete_distribution<> dist;
vector<pair<IncrementallyCheckedTimestamp, IncrementallyCheckedTimestamp>> intervals;
vector<int> found;
vector<bool> foundForCurrentMsg;

void initFreqs()
{
    array<float, NUM_LETTERS> letterFreqs;
    ifstream f("letter_frequencies.txt");
    for (int i = 0; i < NUM_LETTERS; ++i)
        f >> letterFreqs[i];

    dist = discrete_distribution<>(letterFreqs.begin(), letterFreqs.end());
    random_device rd;
    gen.seed(rd());
}

void Message::generate()
{
    //const char *s = "ONEDAYWILLREVEALALL";

    for (int i = 0; i < MSG_LENGTH_PLAIN; ++i)
    {
        //char character = s[i] - 'A' + 1;
        char character = dist(gen) + 1;
        a1z26[2 * i] = character / 10;
        a1z26[2 * i + 1] = character % 10;
    }
}

bool Message::getInt(int pos, int size, int& val) const
{
    if (pos + size > MSG_LENGTH_A1Z26)
        return false;

    val = 0;
    for (int i = 0; i < size; ++i)
        val = val * 10 + a1z26[pos + i];

    return true;
}

ostream& operator<<(ostream& g, const Message& m)
{
    for (int i = 0; i < MSG_LENGTH_A1Z26; ++i)
        g << (int)m.a1z26[i];

    return g;
}

bool IncrementallyCheckedTimestamp::checkShortMonth(int day, int month)
{
    switch (month)
    {
        case 2:
            return day <= 29;
        case 4:
        case 6:
        case 9:
        case 11:
            return day <= 30;
        default:
            return true; /* Already known to be <= 31 */
    }
}

bool IncrementallyCheckedTimestamp::checkLeap(int day, int month, int year)
{
    if (day == 29 && month == 2 && !((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
        return false;

    return true;
}

bool IncrementallyCheckedTimestamp::setField(int pos, int val)
{
    switch (pos)
    {
        /* Checking for any issues asap for a bit more effective pruning. */
        case FieldDay:
            if (val < 1 || val > 31)
                return false;
            if (known[FieldMonth] && !checkShortMonth(val, fields[FieldMonth]))
                return false;
            if (known[FieldMonth] && known[FieldYear] && !checkLeap(val, fields[FieldMonth], fields[FieldYear]))
                return false;
            break;
        case FieldMonth:
            if (val < 1 || val > 12)
                return false;
            if (known[FieldDay] && !checkShortMonth(fields[FieldDay], val))
                return false;
            if (known[FieldDay] && known[FieldYear] && !checkLeap(fields[FieldDay], val, fields[FieldYear]))
                return false;
            break;
        case FieldYear:
            if (val < 0)
                return false;
            if (known[FieldDay] && known[FieldMonth] && !checkLeap(fields[FieldDay], fields[FieldMonth], val))
                return false;
            break;
        case FieldHour:
            if (val < 0 || val > 23)
                return false;
            break;
    }

    fields[pos] = val;
    known[pos] = true;
    return true;
}

void IncrementallyCheckedTimestamp::unsetField(int pos)
{
    known[pos] = false;
}

int IncrementallyCheckedTimestamp::getFieldSize(int fieldType, int fieldSizeType)
{
    if (fieldType == FieldYear)
    {
        if (fieldSizeType == SizeShort)
            return 2;
        return 4;
    }

    if (fieldSizeType == SizeShort)
        return 1;
    return 2;
}

istream& operator>>(istream& f, IncrementallyCheckedTimestamp& t)
{
    int field;

    for (int i = IncrementallyCheckedTimestamp::FieldYear; i < IncrementallyCheckedTimestamp::FieldCount; ++i)
        t.known[i] = false;

    for (int i = IncrementallyCheckedTimestamp::FieldYear; i < IncrementallyCheckedTimestamp::FieldCount; ++i)
    {
        f >> field;
        if (!t.setField(i, field))
        {
            f.setstate(ios::failbit);
            return f;
        }
    }

    return f;
}

bool operator<=(const IncrementallyCheckedTimestamp& t1, const IncrementallyCheckedTimestamp& t2)
{
    for (int i = IncrementallyCheckedTimestamp::FieldYear; i < IncrementallyCheckedTimestamp::FieldCount; ++i)
    {
        if (t1.fields[i] < t2.fields[i])
            return true;

        if (t1.fields[i] > t2.fields[i])
            return false;
    }

    return true;
}

void IncrementallyCheckedTimestamp::processTimestamps(const Message& m, int msgPos, int order, int fieldIndex)
{
    if (fieldIndex == NUM_TIMESTAMP_FIELDS)
    {
        for (unsigned int i = 0; i < intervals.size(); ++i)
            if (intervals[i].first <= *this && *this <= intervals[i].second)
                foundForCurrentMsg[i] = true;

        return;
    }

    auto fieldType = orders[order][fieldIndex];

    for (int sizeType = SizeShort; sizeType < SizeCount; ++sizeType)
    {
        auto size = getFieldSize(fieldType, sizeType);
        int val;

        if (m.getInt(msgPos, size, val))
        {
            if (fieldType == FieldYear && sizeType == SizeShort)
                val += 2000;

            if (setField(fieldType, val))
            {
                processTimestamps(m, msgPos + size, order, fieldIndex + 1);
                unsetField(fieldType);
            }

            /* Uncomment if you input dates between 1900 - 1999. */
            /*if (fieldType == FieldYear && sizeType == SizeShort)
            {
                val -= 100;
                if (setField(fieldType, val))
                {
                    processTimestamps(m, msgPos + size, order, fieldIndex + 1);
                    unsetField(fieldType);
                }
            }*/
        }
    }
}

inline auto getCurrentTimeMs()
{
    auto now = chrono::high_resolution_clock::now();
    return chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
}

int main()
{
    ifstream f("time_intervals.txt");
    pair<IncrementallyCheckedTimestamp, IncrementallyCheckedTimestamp> interval;
    while (f >> interval.first >> interval.second)
    {
        if (!(interval.first <= interval.second))
            cerr << "Warning: Interval #" << intervals.size() + 1 << " has reversed endpoints.\n";
        intervals.push_back(interval);
    }
    if (!f.eof())
    {
        cerr << "Interval #" << intervals.size() + 1 << " is not valid.\n";
        return 1;
    }
    found.resize(intervals.size());
    foundForCurrentMsg.resize(intervals.size());

    initFreqs();

    auto start = getCurrentTimeMs();
    int tests = 0;
    while (true)
    {
        m.generate();

        for (int msgPos = 0; msgPos <= MSG_LENGTH_A1Z26 - MIN_TIMESTAMP_LENGTH; ++msgPos)
            for (int order = 0; order < POSSIBLE_FIELD_ORDERS; ++order)
                currentTimestamp.processTimestamps(m, msgPos, order, 0);

        for (unsigned int i = 0; i < intervals.size(); ++i)
            if (foundForCurrentMsg[i])
            {
                ++found[i];
                foundForCurrentMsg[i] = false;
            }

        ++tests;

        auto now = getCurrentTimeMs();
        if (now - start > 2000)
        {
            //cerr << "\rTests: " << tests << ". Found: ";
            cerr << "\rTests: " << tests << ". Chances: " << fixed << setprecision(3);
            for (unsigned int i = 0; i < intervals.size(); ++i)
                //cerr << found[i] << ' ';
                cerr << found[i] * 100.0 / tests << "% ";

            start = now;
        }
    }

    return 0;
}
