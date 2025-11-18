#include "DBScan.h"

namespace EmbeddingSpaceSampler
{

DBScan::DBScan(double eps, int min_pts, const std::vector<juce::Point<double>>& input_points)
    : eps(eps), min_pts(min_pts), cluster_idx(-1)
{
    size = static_cast<int>(input_points.size());
    adj_points.resize(size);
    
    for (int i = 0; i < size; ++i)
    {
        DBScanPoint d_point;
        d_point.x = input_points[i].x;
        d_point.y = input_points[i].y;
        d_point.pts_cnt = 0;
        d_point.cluster = NOT_CLASSIFIED;
        points.push_back(d_point);
    }
}

void DBScan::run()
{
    check_near_points();
    
    for (int i = 0; i < size; ++i)
    {
        if (points[i].cluster != NOT_CLASSIFIED)
            continue;
        
        if (is_core_object(i))
        {
            dfs(i, ++cluster_idx);
        }
        else
        {
            points[i].cluster = NOISE;
        }
    }
    
    cluster.resize(cluster_idx + 1);
    
    for (int i = 0; i < size; ++i)
    {
        if (points[i].cluster != NOISE)
        {
            cluster[points[i].cluster].push_back(i);
        }
    }
}

void DBScan::dfs(int now, int c)
{
    points[now].cluster = c;
    if (!is_core_object(now))
        return;
    
    for (auto& next : adj_points[now])
    {
        if (points[next].cluster != NOT_CLASSIFIED)
            continue;
        dfs(next, c);
    }
}

void DBScan::check_near_points()
{
    for (int i = 0; i < size; ++i)
    {
        for (int j = 0; j < size; ++j)
        {
            if (i == j)
                continue;
            
            if (points[i].get_distance(points[j]) <= eps)
            {
                points[i].pts_cnt++;
                adj_points[i].push_back(j);
            }
        }
    }
}

bool DBScan::is_core_object(int idx) const
{
    return points[idx].pts_cnt >= min_pts;
}

} // namespace EmbeddingSpaceSampler

