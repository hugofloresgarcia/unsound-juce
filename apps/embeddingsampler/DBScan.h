#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <cmath>

namespace EmbeddingSpaceSampler
{

struct DBScanPoint
{
    double x, y;
    int pts_cnt, cluster;
    
    double get_distance(const DBScanPoint& other) const
    {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

class DBScan
{
public:
    static const int NOISE = -2;
    static const int NOT_CLASSIFIED = -1;
    
    DBScan(double eps, int min_pts, const std::vector<juce::Point<double>>& points);
    
    void run();
    std::vector<std::vector<int>> get_cluster() const { return cluster; }
    
    // Get cluster assignment for a specific point index
    int get_cluster_id(int point_index) const
    {
        if (point_index >= 0 && point_index < static_cast<int>(this->points.size()))
            return points[point_index].cluster;
        return NOT_CLASSIFIED;
    }
    
    // Get number of clusters found
    int get_num_clusters() const { return cluster_idx + 1; }
    
private:
    void dfs(int now, int c);
    void check_near_points();
    bool is_core_object(int idx) const;
    
    double eps;
    int min_pts;
    std::vector<DBScanPoint> points;
    int size;
    std::vector<std::vector<int>> adj_points;
    std::vector<std::vector<int>> cluster;
    int cluster_idx;
};

} // namespace EmbeddingSpaceSampler

