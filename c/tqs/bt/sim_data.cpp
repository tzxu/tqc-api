﻿#include <assert.h>
#include <string.h>
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "sim_data.h"
#include "sim_context.h"

using namespace tquant::api;
using namespace tquant::stralet;
using namespace tquant::stralet::backtest;

#ifdef __linux__

template<typename _Tp, typename _Alloc = std::allocator<_Tp> >
class VectorImpl : public _Vector_base<_Tp,_Alloc>::_Vector_impl { };

static_assert(sizeof(VectorImpl<int>) == sizeof(vector<int>), "vector size is changed!");
              
#endif

static int cmp_time(int date_1, int time_1, int date_2, int time_2)
{
    if (date_1 < date_2) return -1;
    if (date_1 == date_2) return time_1 - time_2;
    return 1;
}

CallResult<const MarketQuoteArray> SimDataApi::tick(const string& code, int trading_day, int number)
{
    if (trading_day == 0 || trading_day == m_ctx->trading_day()) {
        auto it = m_tick_caches.find(code);
        if (it == m_tick_caches.end())
            return CallResult<const MarketQuoteArray>("-1,no tick data");
        if (it->second->pos < 0)
            return CallResult<const MarketQuoteArray>("-1,not arrive yet");
        return CallResult<const MarketQuoteArray>(it->second->ticks);
    }
    else {
        return m_dapi->tick(code, trading_day, number);
    }
}

CallResult<const BarArray> SimDataApi::bar(const string& code, const string& cycle, int trading_day, bool align, int number)
{
    if (cycle != "1m")
        return CallResult<const BarArray>("-1,unsupported cycle");

    if (!align)
        return CallResult<const BarArray>("-1,bactest's bar should be aligned");

    if (trading_day == 0 || trading_day == m_ctx->trading_day()) {
        auto it = m_bar_caches.find(code);
        if (it == m_bar_caches.end())
            return CallResult<const BarArray>("-1,no bar data");
        if (it->second->pos < 0)
            return CallResult<const BarArray>("-1,not arrive yet");
        return CallResult<const BarArray>(it->second->bars);
    }
    else if (trading_day < m_ctx->trading_day())
        return m_dapi->bar(code, cycle, trading_day, align, number);
    else
        return CallResult<const BarArray>("-1,try to get data after current trading_day");
}

CallResult<const DailyBarArray> SimDataApi::daily_bar(const string& code, const string& price_adj, bool align, int number)
{
    // TODO: get it from cache.
    auto r = m_dapi->daily_bar(code, price_adj, align, 0);
    if (r.value) {
        auto today = m_ctx->trading_day();
        auto bars = r.value;
        for (int pos = 0; pos < bars->size(); pos++) {
            if (bars->at(pos).date >= today) {
                int size = bars->at(pos).date > today ? pos - 1 : pos;
                auto new_bars = make_shared<DailyBarArray>(code, size);
                for (int i = 0; i < size; i++)
                    new_bars->push_back(bars->at(i));
                return CallResult<const DailyBarArray>(new_bars);
            }
        }

        // Shouldn't happen?
        return CallResult<const DailyBarArray >("-1,no data");
    }
    else {
        return r;
    }
}

CallResult<const MarketQuote> SimDataApi::quote(const string& code)
{
    if (m_ctx->data_level() == BT_BAR1M) {
        auto it = m_bar_caches.find(code);
        if (it == m_bar_caches.end())
            return CallResult<const MarketQuote>("-1,no bar data");
        if (it->second->pos < 0)
            return CallResult<const MarketQuote>("-1,not arrive yet");
        auto bar = &(it->second->bars->at(it->second->pos));
        auto q = make_shared<MarketQuote>();
        q->set_code(code);
        q->date        = bar->date;
        q->time        = bar->time;
        q->trading_day = bar->trading_day;
        q->oi          = bar->oi;
        q->last = q->ask1 = q->bid1 = bar->close;
        auto dailybar = cur_daily_bar(code);
        q->pre_close  = dailybar ? dailybar->pre_close : 0.0;

        // The close price of last bar is not the close price of the daily bar!
        if (m_is_EOD && dailybar)
            q->last = q->ask1 = q->bid1 = dailybar->close;

        // TODO: get OHLC from bar1m

        return CallResult<const MarketQuote>(q);
    }
    else if (m_ctx->data_level() == BT_TICK) {
        auto it = m_tick_caches.find(code);
        if (it == m_tick_caches.end())
            return CallResult<const MarketQuote>("-1,no tick data");
        if (it->second->pos < 0)
            return CallResult<const MarketQuote>("-1,not arrive yet");
        auto q = make_shared<MarketQuote>(it->second->ticks->at(it->second->pos), code);
        return CallResult<const MarketQuote>(q);
    }
    else {
        return CallResult<const MarketQuote>("-1,support quote when testing using bar1d");
    }
}

void SimDataApi::preload_bar(const vector<string>& codes)
{
    DateTime dt = m_ctx->cur_time();

    for (auto& code : codes) {
        if (m_bar_caches.find(code) != m_bar_caches.end()) continue;
        auto r = m_dapi->bar(code.c_str(), "1m", m_ctx->trading_day(), true, 0);
        if (!r.value) {
            cerr << "no bar data " << m_ctx->trading_day() << "," << code << endl;
            continue;
        }

        auto bars = r.value;
        int pos = -1;
        if (bars->size()) {
            for (pos = 0; pos < bars->size(); pos++) {
                auto b = &bars->at(pos);
                int v = cmp_time(b->date, b->time, dt.date, dt.time);
                if (v == 0)
                    break;
                else if (v > 0) {
                    pos--;
                    break;
                }
            }
            if (pos == bars->size()) {
                pos = bars->size() - 1;
            }
        }
        auto cache = make_shared<BarCache>();
        cache->pos = pos;
        cache->bars = bars;
        cache->size = bars->size();
        // FIXME: Shouldn't use const?
        ((BarArray*)cache->bars.get())->set_size(0);

        m_bar_caches[code] = cache;
    }
}

void SimDataApi::preload_daily_bar(const vector<string>& codes)
{
    for (auto& code : codes) {
        if (m_dailybar_caches.find(code) != m_dailybar_caches.end()) continue;

        // 不复权
        auto r = m_dapi->daily_bar(code.c_str(), "", true, 0);
        if (!r.value) {
            cerr << "no daily_bar data " << code << endl;
            continue;
        }

        auto bars = r.value;
        auto cache = make_shared<DailyBarCache>();
        cache->pos = -1;
        cache->daily_bars = r.value;

        int trading_day = m_ctx->trading_day();
        for (int i = 0; i < bars->size(); i++) {
            if (bars->at(i).date <= trading_day)
                cache->pos = i;
            else
                break;
        }

        if (cache->pos == -1) cache->pos = 0;
        //assert(cache.pos != -1);

        m_dailybar_caches[code] = cache;
    }
}

void SimDataApi::preload_tick(const vector<string>& codes)
{
    DateTime dt = m_ctx->cur_time();

    for (auto& code : codes) {
        if (m_tick_caches.find(code) != m_tick_caches.end()) continue;
        auto r = m_dapi->tick(code.c_str(), m_ctx->trading_day(), 0);
        if (!r.value) continue;

        auto ticks = r.value;
        int pos = -1;
        if (ticks->size() > 0) {
            for (pos = 0; pos < (int)ticks->size(); pos++) {
                auto a = &ticks->at(pos);
                int v = cmp_time(a->date, a->time, dt.date, dt.time);
                if (v == 0) break;
                else if (v > 0) {
                    pos--; break;
                }
            }
            if (pos == (int)ticks->size())
                pos = ticks->size() - 1;
        }

        auto cache = make_shared<TickCache>();
        cache->pos = pos;
        cache->ticks = ticks;
        cache->size = ticks->size();
        ((TickArray*)cache->ticks.get())->set_size(0);
        m_tick_caches[code] = cache;
    }

}

void SimDataApi::pin_code(const string& code)
{
    if (m_pinned_codes.find(code) != m_pinned_codes.end()) return;
    m_pinned_codes.insert(code);

    if (m_ctx->data_level() == BT_BAR1M || m_ctx->data_level() == BT_TICK) {
        preload_daily_bar(vector<string>{code});
        preload_bar(vector<string>{code});
    }

    if (m_ctx->data_level() == BT_TICK) {
        preload_tick(vector<string>{code});
    }
}

void SimDataApi::unpin_code(const string& code)
{
    auto it = m_pinned_codes.find(code);
    if (it== m_pinned_codes.end()) return;

    m_pinned_codes.erase(it);

    if (m_codes.find(code) == m_codes.end()) {
        auto it1 = m_bar_caches.find(code);
        if (it1 != m_bar_caches.end()) m_bar_caches.erase(it1);
        auto it2 = m_tick_caches.find(code);
        if (it2 != m_tick_caches.end()) m_tick_caches.erase(it2);
    }
}

CallResult<const vector<string>> SimDataApi::subscribe(const vector<string>& old_codes)
{
    vector<string> new_codes;
    for (auto code : old_codes)
        if (!code.empty()) new_codes.push_back(code);

    m_dapi->subscribe(new_codes);

    if (m_ctx->data_level() == BT_BAR1M || m_ctx->data_level() == BT_TICK) {
        preload_daily_bar(new_codes);
        preload_bar(new_codes);
    }

    if (m_ctx->data_level() == BT_TICK) {
        preload_tick(new_codes);
    }

    for (auto& code : new_codes) m_codes.insert(code);

    auto ret_codes = make_shared<vector<string>>();
    for (auto& c : new_codes) ret_codes->push_back(c);
    
    sort(ret_codes->begin(), ret_codes->end());

    return CallResult<const vector<string>>(ret_codes);
}

CallResult<const vector<string>> SimDataApi::unsubscribe(const vector<string>& codes)
{
    for (auto & c : codes) {
        // Reserve data for pinned code
        if (m_pinned_codes.find(c) != m_pinned_codes.end()) continue;

        auto it1 = m_bar_caches.find(c);
        if (it1 != m_bar_caches.end()) m_bar_caches.erase(it1);
        auto it2 = m_tick_caches.find(c);
        if (it2 != m_tick_caches.end()) m_tick_caches.erase(it2);

        auto it3 = m_codes.find(c);
        if (it3 != m_codes.end()) m_codes.erase(it3);
    }

    auto ret_codes = make_shared<vector<string>>();
    for (auto& c : codes) ret_codes->push_back(c);
    sort(ret_codes->begin(), ret_codes->end());

    return CallResult<const vector<string>>(ret_codes);
}

DataApi_Callback* SimDataApi::set_callback(DataApi_Callback* callback)
{
    // TODO:
    return nullptr;
}

static inline bool equal(int64_t f1, int64_t f2) {
    return f1 == f2;
}

static inline bool equal(double f1, double f2, double precise=1e-8) {
    return fabs(f1-f2) < precise;
}


static inline bool is_duplicated_quote(const RawMarketQuote* q1, const RawMarketQuote* q2) 
{
    return q1->time == q2->time &&
        equal(q1->turnover, q2->turnover) && 
        equal(q1->volume, q2->volume) && 
        equal(q1->ask1, q2->ask1) && 
        equal(q1->ask2, q2->ask2) && 
        equal(q1->ask3, q2->ask3) && 
        equal(q1->ask4, q2->ask4) && 
        equal(q1->ask5, q2->ask5) && 
        equal(q1->ask1, q2->ask1) && 
        equal(q1->bid1, q2->bid1) && 
        equal(q1->bid2, q2->bid2) && 
        equal(q1->bid3, q2->bid3) && 
        equal(q1->bid4, q2->bid4) && 
        equal(q1->bid5, q2->bid5) && 
        equal(q1->ask_vol1, q2->ask_vol1) && 
        equal(q1->ask_vol2, q2->ask_vol2) && 
        equal(q1->ask_vol3, q2->ask_vol3) && 
        equal(q1->ask_vol4, q2->ask_vol4) && 
        equal(q1->ask_vol5, q2->ask_vol5) && 
        equal(q1->bid_vol1, q2->bid_vol1) && 
        equal(q1->bid_vol2, q2->bid_vol2) && 
        equal(q1->bid_vol3, q2->bid_vol3) && 
        equal(q1->bid_vol4, q2->bid_vol4) && 
        equal(q1->bid_vol5, q2->bid_vol5) && 
        equal(q1->last, q2->last);
}

void SimDataApi::calc_nex_time(DateTime* dt)
{
    int32_t date = m_ctx->trading_day();
    int32_t time = 160000000;

    for (auto& e : m_bar_caches) {
        auto cache = e.second;
        //const Bar* bar = (cache->pos + 1 < cache->size) ?
        //                cache->bars->data() + cache->pos + 1 :
        //                cache->bars->data() + cache->pos;

        if (cache->pos + 1 < cache->size) {
            auto bar = &cache->bars->at(cache->pos + 1);
            if (cmp_time(date, time, bar->date, bar->time) > 0) {
                date = bar->date;
                time = bar->time;
            }
        }
    }

    for (auto& e : m_tick_caches) {
        auto cache = e.second;
        while (cache->pos >= 0 && cache->pos + 1 < cache->size) {
            auto q1 = &cache->ticks->at(cache->pos);
            auto q2 = &cache->ticks->at(cache->pos + 1);
            if (is_duplicated_quote(q1, q2)) {
                cache->pos += 1;
                continue;
            } 
            else if (cmp_time(q2->date, q2->time, q1->date, q1->time) <= 0) {
                // XXXX shouldn't ignore bad quote only by timestamp!
                cache->pos += 1;
                continue;
            }
            else {
                break;
            }
        }
        if (cache->pos + 1 < cache->size) {
            auto q = &cache->ticks->at(cache->pos + 1);

            if (cmp_time(date, time, q->date, q->time) > 0) {
                date = q->date;
                time = q->time;
            }
        }
    }

    dt->date = date;
    dt->time = time;
}

shared_ptr<MarketQuote> SimDataApi::prev_quote(const string& code)
{
    auto it = m_tick_caches.find(code);
    if (it == m_tick_caches.end())
        return nullptr;

    auto cache = it->second;
    DateTime dt = m_ctx->cur_time();

    if (cache->pos >= 1) {
        auto q = &cache->ticks->at(cache->pos - 1);
        return make_shared<MarketQuote>(*q);
    }
    else {
        return nullptr;
    }
}

shared_ptr<MarketQuote> SimDataApi::next_quote(const string& code)
{
    auto it = m_tick_caches.find(code);
    if (it == m_tick_caches.end())
        return nullptr;

    auto cache = it->second;
    DateTime dt = m_ctx->cur_time();

    if (cache->pos + 1 >= cache->size) return nullptr;
    auto q = &cache->ticks->at(cache->pos + 1);
    if (cmp_time(q->date, q->time, dt.date, dt.time) <= 0) {
        cache->pos += 1;
        ((TickArray*)cache->ticks.get())->set_size(cache->pos+1);
        return make_shared<MarketQuote>(*q);
    }
    else {
        return nullptr;
    }
}

shared_ptr<Bar> SimDataApi::next_bar(const string& code)
{
    auto it = m_bar_caches.find(code);
    if (it == m_bar_caches.end())
        return nullptr;

    auto cache = it->second;
    DateTime dt = m_ctx->cur_time();

    if (cache->pos + 1 >= cache->size) return nullptr;
    auto bar = &cache->bars->at(cache->pos + 1);
    if (cmp_time(bar->date, bar->time, dt.date, dt.time) <= 0) {
        cache->pos += 1;
        ((TickArray*)cache->bars.get())->set_size(cache->pos + 1);
        return make_shared<Bar>(*bar);
    }
    else {
        return nullptr;
    }
}

void SimDataApi::set_end_of_day()
{
    m_is_EOD = true;
}

void SimDataApi::set_data_to_curtime()
{
    DateTime dt = m_ctx->cur_time();

    for (auto it = m_bar_caches.begin(); it != m_bar_caches.end(); it++) {
        auto cache = it->second;
        while(true) {
            if (cache->pos + 1 >= cache->size) break;
            auto bar = &cache->bars->at(cache->pos + 1);
            if (cmp_time(bar->date, bar->time, dt.date, dt.time) <= 0) {
                cache->pos += 1;
                ((TickArray*)cache->bars.get())->set_size(cache->pos + 1);
            }
            else {
                break;
            }
        }
    }

    for (auto it = m_tick_caches.begin(); it != m_tick_caches.end(); it++) {
        auto cache = it->second;
        while (true) {
            if (cache->pos + 1 >= cache->size) break;
            auto bar = &cache->ticks->at(cache->pos + 1);
            if (cmp_time(bar->date, bar->time, dt.date, dt.time) <= 0) {
                cache->pos += 1;
                ((TickArray*)cache->ticks.get())->set_size(cache->pos + 1);
            }
            else {
                break;
            }
        }
    }
}

const RawBar* SimDataApi::last_bar(const string & code)
{
    auto it = m_bar_caches.find(code);
    if (it == m_bar_caches.end())
        return nullptr;

    auto cache = it->second;
    return cache->pos >= 0 ? &cache->bars->at(cache->pos) : nullptr;
}

const RawDailyBar* SimDataApi::cur_daily_bar(const string & code)
{
    auto it = m_dailybar_caches.find(code);
    if (it == m_dailybar_caches.end())
        return nullptr;

    auto cache = it->second;
    if (cache->pos >=0 && cache->pos < (int)cache->daily_bars->size()) {
        auto bar = &cache->daily_bars->at(cache->pos);
        if (bar->date == m_ctx->trading_day())
            return bar;
    }

    return nullptr;
}

void SimDataApi::move_to(int trading_day)
{
    m_is_EOD = false;
    this->m_tick_caches.clear();
    this->m_bar_caches.clear();
    this->m_codes.clear();

    // Move to next trading_day !
    for (auto tmp : this->m_dailybar_caches) {
        auto bar = tmp.second;
        while (bar->pos < (int)bar->daily_bars->size()) {
            if (bar->daily_bars->at(bar->pos).date >= trading_day) break;
            bar->pos++;
        }
    }
}
