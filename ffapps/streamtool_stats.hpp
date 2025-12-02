/**
 * \verbatim
 * flipflip's c++ apps (ffapps)
 *
 * Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
 * https://oinkzwurgl.org/projaeggd/ffxx/
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 * \endverbatim
 *
 * @file
 * @brief streamtool message analysis and statistics
 */
#ifndef __FFAPPS_STREAMTOOL_STREAMTOOL_STATS_HPP__
#define __FFAPPS_STREAMTOOL_STREAMTOOL_STATS_HPP__

/* LIBC/STL */
#include <array>
#include <cmath>
#include <map>
#include <string>

/* EXTERNAL */
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/extended_p_square.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/histogram.hpp>
#include <ffxx/epoch.hpp>
#include <fpsdk_common/parser/types.hpp>

/* PACKAGE */

namespace ffapps::streamtool {
/* ****************************************************************************************************************** */

class Stats
{
   public:
    Stats();

    struct MsgInfo
    {
        std::size_t offs_ = 0;
        std::string unique_name_;
        fpsdk::common::time::Time recv_ts_;
        fpsdk::common::time::Time data_ts_;
        fpsdk::common::time::Duration latency_;
        fpsdk::common::time::Duration interval_;
    };

    MsgInfo Update(const fpsdk::common::parser::ParserMsg& msg, const fpsdk::common::time::Time& recv_ts);
    MsgInfo Update(const ffxx::Epoch& epoch, const fpsdk::common::time::Time& recv_ts);

    struct MsgStats
    {
        // clang-format off
        std::string                            protocol_name_;
        std::string                            message_name_;
        std::string                            unique_name_;
        std::string                            msg_desc_;
        fpsdk::common::time::Time              last_data_ts_;
        fpsdk::common::time::Time              last_recv_ts_;
        std::size_t                            count_ = 0;
        std::size_t                            bytes_ = 0;

        // Sanity checks: ignore samples outside these ranges
        static constexpr double SANITY_LATENCY_MIN    =  -1.0;
        static constexpr double SANITY_LATENCY_MAX    =   2.0;
        static constexpr double SANITY_INTERVAL_MIN   =   0.0;
        static constexpr double SANITY_INTERVAL_MAX   =  60.0;
        static constexpr double SANITY_FREQUENCY_MIN  =   0.0;
        static constexpr double SANITY_FREQUENCY_MAX  = 250.0;

        boost::histogram::histogram<std::tuple<boost::histogram::axis::regular<>>> hist_latency_
            = boost::histogram::make_histogram(boost::histogram::axis::regular<>(200, -0.011, 0.189));

        boost::histogram::histogram<std::tuple<boost::histogram::axis::regular<>>> hist_interval_
            = boost::histogram::make_histogram(boost::histogram::axis::regular<>(200, 0.0025, 1.0025));

        boost::histogram::histogram<std::tuple<boost::histogram::axis::regular<>>> hist_frequency_
            = boost::histogram::make_histogram(boost::histogram::axis::regular<>(200, 0.5, 200.5));

        static constexpr std::array<double, 4> PROB = {{0.5, 0.68, 0.95, 0.997}}; // median, 1/2/3 sigma
        using Accumulator = boost::accumulators::accumulator_set<double, boost::accumulators::stats<
            boost::accumulators::tag::count,
            boost::accumulators::tag::mean,
            boost::accumulators::tag::min,
            boost::accumulators::tag::max,
            boost::accumulators::tag::variance,
            boost::accumulators::tag::extended_p_square>>;
        Accumulator acc_latency_   { boost::accumulators::extended_p_square_probabilities = PROB };
        Accumulator acc_interval_  { boost::accumulators::extended_p_square_probabilities = PROB };
        Accumulator acc_frequency_ { boost::accumulators::extended_p_square_probabilities = PROB };

        // clang-format on
    };

    std::map<std::string, MsgStats> msg_stats_;

   private:
    std::size_t offs_ = 0;

    void AnalyseFpa(const fpsdk::common::parser::ParserMsg& msg, MsgInfo& info);
    void AnalyseUbx(const fpsdk::common::parser::ParserMsg& msg, MsgInfo& info);
    void AnalyseRtcm3(const fpsdk::common::parser::ParserMsg& msg, MsgInfo& info);
};

/* ****************************************************************************************************************** */
}  // namespace ffapps::streamtool
#endif  // __FFAPPS_STREAMTOOL_STREAMTOOL_STATS_HPP__
