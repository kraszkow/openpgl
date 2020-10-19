// Copyright 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../rkguide.h"
#include "KDTree.h"
#include "../data/SampleStatistics.h"
#include "../data/Range.h"

#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>

#include <iostream>
#include <limits>

namespace rkguide
{

template<typename TSamples, typename TRegion, typename TRange>
struct KDTreePartitionBuilder
{

    struct Settings
    {
        size_t minSamples {100};
        size_t maxSamples {32000};
        size_t maxDepth{32};
    };

    void build(KDTree &kdTree, const BBox &bound, std::vector<TSamples> &samples, tbb::concurrent_vector< std::pair<TRegion, TRange> > &dataStorage, const Settings &buildSettings) const
    {

        kdTree.init(bound, 4096);
        dataStorage.resize(1);

        int numEstLeafs = (samples.size()*2)/buildSettings.maxSamples+32;
        kdTree.m_nodes.reserve(4*numEstLeafs);
        dataStorage.reserve(2*numEstLeafs);

        //KDNode &root kdTree.getRoot();
        updateTree(kdTree, samples, dataStorage, buildSettings);
    }

    void updateTree(KDTree &kdTree, std::vector<TSamples> &samples, tbb::concurrent_vector< std::pair<TRegion, TRange> > &dataStorage, const Settings &buildSettings) const
    {
        KDNode &root = kdTree.getRoot();
        SampleStatistics sampleStats;
        sampleStats.clear();
        //size_t numSamples = samples.size();

        TRange sampleRange;
        sampleRange.start = samples.begin();
        sampleRange.end = samples.end();

        size_t depth =1;


        if (root.isLeaf())
        {
            mitsuba::ref<mitsuba::Timer> statsTimer = new mitsuba::Timer();

            double x = 0.0f;
            double y = 0.0f;
            double z = 0.0f;

            for (const auto& sample : samples)
            {
                sampleStats.addSample(sample.position);
                x += sample.position[0];
                y += sample.position[1];
                z += sample.position[2];
            }

            x /= double(samples.size());
            y /= double(samples.size());
            z /= double(samples.size());
/*
            std::cout <<  "Stats building: time: " <<  statsTimer->getSeconds() << std::endl;

            mitsuba::ref<mitsuba::Timer> statsTimerPar = new mitsuba::Timer();
            SampleStatistics sampleStatsIdentity;
            SampleStatistics sampleStats2 = tbb::parallel_reduce(
                tbb::blocked_range<int>(0,samples.size()), 
                sampleStatsIdentity, 
                [&](tbb::blocked_range<int> r, SampleStatistics running_total)
                {
                        for (int i=r.begin(); i<r.end(); ++i)
                        {
                            running_total.addSample(samples[i].position);
                        }
                        return running_total;
                    }, SampleStatistics());
            //std::cout <<  "StatsPar building: time: " <<  statsTimerPar->getSeconds() << std::endl;
            std::cout <<  "mean double: " << x << "\t" << y << "\t" << z << std::endl;
            std::cout <<  "sampleStats: " << sampleStats.toString() << std::endl;
            std::cout <<  "sampleStats2: " << sampleStats2.toString() << std::endl;

            //std::cout <<  "Tree building: time: " <<  statsTimer->getSeconds() << std::endl;
            sampleStats = sampleStats2;
*/
        }
        std::cout <<  std::numeric_limits<float>::max() << "\t" << sampleStats.getNumSamples() << std::endl;
#pragma omp parallel num_threads(20)
#pragma omp single nowait
        updateTreeNode(&kdTree, root, depth/*, samples*/, sampleRange, sampleStats, &dataStorage, buildSettings);

        //std::cout << "KDTree: numNodes = " << kdTree.getNumNodes() << std::endl;
        //std::cout << "DataStorage: numData = " << dataStorage.size() << std::endl;

    }

private:


    typename std::vector<TSamples>::iterator pivotSplitSamplesWithStats(typename std::vector<TSamples>::iterator begin,
                                                                           typename std::vector<TSamples>::iterator end,
                                                                            uint8_t splitDimension, float pivot, SampleStatistics &statsLeft, SampleStatistics &statsRight) const
    {
        std::function<bool(TSamples)> pivotSplitPredicate
                = [splitDimension, pivot, &statsLeft, &statsRight](TSamples sample) -> bool
        {
            bool left = sample.position[splitDimension] < pivot;
            if(left){
                statsLeft.addSample(sample.position);
            }else{
                statsRight.addSample(sample.position);
            }
            return left;
        };
        return std::partition(begin, end, pivotSplitPredicate);
    }


    void getSplitDimensionAndPosition(const SampleStatistics &sampleStats, uint8_t &splitDim, float &splitPos) const
    {
        const Vector3 sampleVariance = sampleStats.getVaraince();
        const Point3 sampleMean = sampleStats.getMean();

        auto maxDimension = [](const Vector3& v) -> uint8_t
        {
            return v[v[1] > v[0]] > v[2] ? v[1] > v[0] : 2;
        };

        splitDim = maxDimension(sampleVariance);
        splitPos = sampleMean[splitDim];
    }

    void updateTreeNode(KDTree *kdTree, KDNode &node, size_t depth, const TRange sampleRange, const SampleStatistics sampleStats, tbb::concurrent_vector< std::pair<TRegion, TRange> > *dataStorage, const Settings &buildSettings) const
    {
        if(sampleRange.size() == 0)
        {
            return;
        }
        //std::cout << "updateTreeNode: " << "depth: " << depth << " \t sampleRange: " <<  sampleRange.start << " | " << sampleRange.end << std::endl;
        uint8_t splitDim = {0};
        float splitPos = {0.0f};

        uint32_t nodeIdsLeftRight[2];
        TRange sampleRangeLeftRight[2];
        SampleStatistics sampleStatsLeftRight[2];

        if (node.isLeaf())
        {
            uint32_t dataIdx = node.getDataIdx();
            std::pair<TRegion, TRange> &regionAndRangeData = dataStorage->at(dataIdx);
            if(depth < buildSettings.maxDepth && regionAndRangeData.first.sampleStatistics.numSamples + sampleRange.size() > buildSettings.maxSamples)
            {
                //regionAndRangeData.first.onSplit();
                regionAndRangeData.first.sampleStatistics.decay(0.5f);
                auto rigthDataItr = dataStorage->push_back(regionAndRangeData);
                uint32_t rightDataIdx = std::distance(dataStorage->begin(), rigthDataItr);
                //std::cout << "rightDataIdx: " << rightDataIdx << "\t" << rigthDataItr - dataStorage->begin() << std::endl;
                getSplitDimensionAndPosition(sampleStats, splitDim, splitPos);
                //we need to split the leaf node
                nodeIdsLeftRight[0] = kdTree->addChildrenPair();
                nodeIdsLeftRight[1] = nodeIdsLeftRight[0] + 1;
                node.setToInnerNode(splitDim, splitPos, nodeIdsLeftRight[0]);
                kdTree->getNode(nodeIdsLeftRight[0]).setDataNodeIdx(dataIdx);
                kdTree->getNode(nodeIdsLeftRight[1]).setDataNodeIdx(rightDataIdx);

                RKGUIDE_ASSERT( kdTree.getNode(nodeIdsLeftRight[0]).isLeaf() );
                RKGUIDE_ASSERT( kdTree.getNode(nodeIdsLeftRight[1]).isLeaf() );
            }
            else
            {
                //std::cout << "\t\tNo Split" << std::endl;
                //std::cout << "dataId: " << dataIdx << "\tdepth: " << depth << "\tsize: " << sampleRange.size()  << std::endl;
                regionAndRangeData.second = sampleRange;
                return;
            }
        }
        else
        {
            splitDim = node.getSplitDim();
            splitPos = node.getSplitPivot();
            nodeIdsLeftRight[0] = node.getLeftChildIdx();
            nodeIdsLeftRight[1] = nodeIdsLeftRight[0] + 1;
        }

        RKGUIDE_ASSERT( !node.isLeaf() );
        RKGUIDE_ASSERT (sampleRange.size() > 0);
        // TODO: update sample stats
        sampleStatsLeftRight[0].clear();
        sampleStatsLeftRight[1].clear();
        auto rPivotItr = pivotSplitSamplesWithStats(sampleRange.start, sampleRange.end, splitDim, splitPos, sampleStatsLeftRight[0], sampleStatsLeftRight[1]);

        //size_t pivotOffSet  = std::distance( samples.begin(), rPivotItr );
        RKGUIDE_ASSERT (pivotOffSet > 0);
        RKGUIDE_ASSERT (pivotOffSet > std::distance(samples.begin(), sampleRange.start));
        RKGUIDE_ASSERT (pivotOffSet < std::distance(samples.begin(),sampleRange.end));
        sampleRangeLeftRight[0].start = sampleRange.start;
        sampleRangeLeftRight[0].end = rPivotItr;

        sampleRangeLeftRight[1].start = rPivotItr;
        sampleRangeLeftRight[1].end = sampleRange.end;

        RKGUIDE_ASSERT(sampleRangeLeftRight[0].size() > 1);
        RKGUIDE_ASSERT(sampleRangeLeftRight[1].size() > 1);

/* */
#pragma omp task mergeable
        updateTreeNode(kdTree, kdTree->getNode(nodeIdsLeftRight[0]), depth + 1, sampleRangeLeftRight[0], sampleStatsLeftRight[0], dataStorage, buildSettings);
        updateTreeNode(kdTree, kdTree->getNode(nodeIdsLeftRight[1]), depth + 1, sampleRangeLeftRight[1], sampleStatsLeftRight[1], dataStorage, buildSettings);
/* */

/* 
#pragma omp parallel for //mergeable
        for (size_t i = 0; i < 2; i++)
        {
            updateTreeNode(kdTree, kdTree.getNode(nodeIdsLeftRight[i]), depth + 1, sampleRangeLeftRight[i], sampleStatsLeftRight[i], dataStorage, buildSettings);
        }
*/
    }

};

}