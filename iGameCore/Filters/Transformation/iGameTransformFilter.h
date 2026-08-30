#ifndef iGameTransformFilter_h
#define iGameTransformFilter_h

#include "iGameFilter.h"
#include "iGamePointSet.h"

IGAME_NAMESPACE_BEGIN

class TransformFilter : public Filter
{
public:
    I_OBJECT(TransformFilter)

    static Pointer New(){return new TransformFilter;}

    void SetMatrix(const float matrix[4][4]);

    void SetTranslation(float tx,float ty,float tz);

    void SetRotation(float rx,float ry,float rz);

    void SetScale(float sx,float sy,float sz);

    void BuildMatrix();

    bool Execute() override;


protected:

    TransformFilter();

    ~TransformFilter() override = default;

    float m_Matrix[4][4]{};
    
    float m_ScaleX{1.0f};
    float m_ScaleY{1.0f};
    float m_ScaleZ{1.0f};

    float m_TranslateX{0.0f};
    float m_TranslateY{0.0f};
    float m_TranslateZ{0.0f};

    float m_RotationX{0.0f};
    float m_RotationY{0.0f};
    float m_RotationZ{0.0f};
};

IGAME_NAMESPACE_END

#endif