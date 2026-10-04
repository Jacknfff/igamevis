#include "iGameTemporalShiftScaleFilter.h"

#include "iGameAttributeSet.h"
#include "iGameDrawObject.h"
#include "iGamePointSet.h"
#include "iGameStringArray.h"
#include "iGameStructuredMesh.h"
#include "iGameSurfaceMesh.h"
#include "iGameType.h"
#include "iGameUnstructuredMesh.h"
#include "iGameVolumeMesh.h"

#include <cmath>
#include <cstddef>
#include <vector>

IGAME_NAMESPACE_BEGIN

TemporalShiftScaleFilter::TemporalShiftScaleFilter() {
    SetNumberOfInputs(1);
    SetNumberOfOutputs(1);
}

DataObject::Pointer TemporalShiftScaleFilter::CreateOutputLike(const DataObject::Pointer& input) {
    if (input == nullptr) { return nullptr; }

    switch (input->GetDataObjectType()) {
        case IG_SURFACE_MESH: {
            auto inputMesh = DynamicCast<SurfaceMesh>(input);
            auto outputMesh = SurfaceMesh::New();
            if (inputMesh.IsNotNull()) {
                // SurfaceMesh 是目前唯一提供浅拷贝的网格类型（共享 points / faces / attributes）
                outputMesh->ShallowCopy(inputMesh);
            }
            return outputMesh;
        }
        case IG_UNSTRUCTURED_MESH: {
            auto inputMesh = DynamicCast<UnstructuredMesh>(input);
            auto outputMesh = UnstructuredMesh::New();
            if (inputMesh.IsNotNull()) {
                outputMesh->SetPoints(inputMesh->GetPoints());
                outputMesh->SetCells(inputMesh->GetCells(), inputMesh->GetCellTypes());
                outputMesh->SetAttributeSet(inputMesh->GetAttributeSet());
            }
            return outputMesh;
        }
        case IG_VOLUME_MESH: {
            auto inputMesh = DynamicCast<VolumeMesh>(input);
            auto outputMesh = VolumeMesh::New();
            if (inputMesh.IsNotNull()) {
                outputMesh->SetVolumes(inputMesh->GetVolumes());
                outputMesh->SetPoints(inputMesh->GetPoints());
                outputMesh->SetAttributeSet(inputMesh->GetAttributeSet());
            }
            return outputMesh;
        }
        case IG_STRUCTURED_MESH: {
            auto inputMesh = DynamicCast<StructuredMesh>(input);
            auto outputMesh = StructuredMesh::New();
            if (inputMesh.IsNotNull()) {
                outputMesh->SetDimensionSize(inputMesh->GetDimensionSize());
                outputMesh->SetExtent(inputMesh->GetExtent());
                outputMesh->SetPoints(inputMesh->GetPoints());
                outputMesh->SetAttributeSet(inputMesh->GetAttributeSet());
            }
            return outputMesh;
        }
        case IG_POINT_SET: {
            auto inputMesh = DynamicCast<PointSet>(input);
            auto outputMesh = PointSet::New();
            if (inputMesh.IsNotNull()) {
                outputMesh->SetPoints(inputMesh->GetPoints());
                outputMesh->SetAttributeSet(inputMesh->GetAttributeSet());
            }
            return outputMesh;
        }
        case IG_DRAW_OBJECT: {
            // .pvd / .igcm 读出来的是 DrawObject 容器：几何在 sub-data-objects 里，这里一并共享
            auto inputObject = DynamicCast<DrawObject>(input);
            auto outputObject = DrawObject::New();
            if (inputObject.IsNull()) { return outputObject; }

            outputObject->SetAttributeSet(inputObject->GetAttributeSet());
            outputObject->SetColorMapper(inputObject->GetColorMapper());
            outputObject->SetViewStyle(static_cast<IGenum>(inputObject->GetViewStyle()));
            outputObject->SetPointSize(static_cast<float>(inputObject->GetPointSize()));
            outputObject->SetLineWidth(static_cast<float>(inputObject->GetLineWidth()));
            outputObject->SetTransparency(inputObject->GetTransparency());
            outputObject->SetVisibility(inputObject->GetVisibility());
            outputObject->SetAttributeIndex(inputObject->GetAttributeIndex());

            // 共享"当前这一帧"的子对象（只读）。AddSubDataObject() 内部会把子对象的
            // parent / colorMapper 改写成新的输出容器，搬完必须还原，
            // 否则输入对象在模型树里的父子关系会被破坏（FileIO 写 IGCM 时依赖 FindParent）。
            std::vector<DataObject::Pointer> children;
            children.reserve(static_cast<std::size_t>(inputObject->GetNumberOfSubDataObjects()));
            for (auto it = inputObject->SubDataObjectIteratorBegin(); it != inputObject->SubDataObjectIteratorEnd();
                 ++it) {
                children.push_back(it->second);
            }
            for (auto& child : children) {
                if (child == nullptr) { continue; }
                auto originalColorMapper = child->GetColorMapper();
                outputObject->AddSubDataObject(child);
                child->SetParentDataObject(inputObject.GetPointer());
                child->SetColorMapper(originalColorMapper);
            }
            return outputObject;
        }
        default:
            break;
    }

    // 其它类型：退化为基类对象（仍然只换时间轴、共享属性）
    auto output = DataObject::CreateDataObject(input->GetDataObjectType());
    if (output == nullptr) { output = DataObject::New(); }
    output->SetAttributeSet(input->GetAttributeSet());
    return output;
}

bool TemporalShiftScaleFilter::Execute() {
    auto input = this->GetInput(0);
    if (input == nullptr) {
        igDebug("TemporalShiftScaleFilter: 输入为空");
        return false;
    }

    auto inputFrames = input->PeekTimeFrames();
    if (inputFrames == nullptr || inputFrames->GetTimeNum() == 0) {
        igDebug("TemporalShiftScaleFilter: 输入没有时间序列（TimeFrames 为空）");
        return false;
    }
    if (std::abs(m_Scale) < 1e-12f) {
        igDebug("TemporalShiftScaleFilter: Scale 不能为 0");
        return false;
    }

    const int timeStepCount = static_cast<int>(inputFrames->GetTimeNum());
    auto outputFrames = StreamingData::New();

    m_InTimeValues.clear();
    m_OutTimeValues.clear();
    m_InTimeValues.reserve(static_cast<std::size_t>(timeStepCount));
    m_OutTimeValues.reserve(static_cast<std::size_t>(timeStepCount));

    for (int index = 0; index < timeStepCount; ++index) {
        auto& inputFrame = inputFrames->GetTargetTimeFrame(static_cast<unsigned int>(index));

        const float inValue = inputFrame.GetTimeValue();
        const float outValue = (inValue + m_PreShift) * m_Scale + m_PostShift;
        outputFrames->AddTimeStep(outValue, inputFrame.GetMetaData(), inputFrame.GetFrameType());

        // 已经读进内存的帧：连缓存数据一起带过去
        if (inputFrame.GetISCached()) {
            const auto newIndex = static_cast<unsigned int>(outputFrames->GetTimeNum() - 1);
            outputFrames->GetTargetTimeFrame(newIndex).SetCache(inputFrame.GetCachedData());
        }

        m_InTimeValues.push_back(inValue);
        m_OutTimeValues.push_back(outValue);
    }

    // 保留输入的缓存策略：StreamingData 没有拷贝接口，用最大缓存帧数复现
    if (inputFrames->GetMaxCacheSize() > 0) {
        outputFrames->EnableCache(inputFrames->GetMaxCacheSize());
    }

    auto output = CreateOutputLike(input);
    if (output == nullptr) {
        igDebug("TemporalShiftScaleFilter: 创建输出对象失败");
        return false;
    }
    output->SetName(input->GetName());
    output->SetTimeFrames(outputFrames);

    this->SetOutput(output);
    return true;
}

IGAME_NAMESPACE_END
