#include "stdafx.h"
#include "xrSkinning.h"
#include "../Layers/xrRender/SkeletonXVertRender.h"
#include <xmmintrin.h>
#include <tbb/tbb.h>
#include <tbb/task_group.h>
#include <tbb/blocked_range.h>

// Helper to transform point P by matrix M
inline __m128 TransformPoint(const Fvector& P, const Fmatrix& M) {
    __m128 row0 = _mm_loadu_ps(&M._11);
    __m128 row1 = _mm_loadu_ps(&M._21);
    __m128 row2 = _mm_loadu_ps(&M._31);
    __m128 row3 = _mm_loadu_ps(&M._41);

    __m128 x = _mm_set1_ps(P.x);
    __m128 y = _mm_set1_ps(P.y);
    __m128 z = _mm_set1_ps(P.z);

    // x*row0 + y*row1 + z*row2 + row3
    __m128 res = _mm_add_ps(_mm_mul_ps(x, row0), _mm_mul_ps(y, row1));
    res = _mm_add_ps(res, _mm_mul_ps(z, row2));
    res = _mm_add_ps(res, row3);
    return res;
}

// Helper to transform direction N by matrix M (ignoring translation)
inline __m128 TransformDir(const Fvector& N, const Fmatrix& M) {
    __m128 row0 = _mm_loadu_ps(&M._11);
    __m128 row1 = _mm_loadu_ps(&M._21);
    __m128 row2 = _mm_loadu_ps(&M._31);
    
    __m128 x = _mm_set1_ps(N.x);
    __m128 y = _mm_set1_ps(N.y);
    __m128 z = _mm_set1_ps(N.z);

    // x*row0 + y*row1 + z*row2
    __m128 res = _mm_add_ps(_mm_mul_ps(x, row0), _mm_mul_ps(y, row1));
    res = _mm_add_ps(res, _mm_mul_ps(z, row2));
    return res;
}

void xrSkin1W_SSE_Internal(vertRender* D, vertBoned1W* S, u32 vCount, CBoneInstance* Bones) {
    for (u32 i = 0; i < vCount; ++i) {
        const vertBoned1W& src = S[i];
        vertRender& dst = D[i];

        const Fmatrix& M0 = Bones[src.matrix].mRenderTransform;

        __m128 P_sum = TransformPoint(src.P, M0);
        __m128 N_sum = TransformDir(src.N, M0);

        _mm_storeu_ps((float*)&dst.P, P_sum);
        _mm_storeu_ps((float*)&dst.N, N_sum);
        
        dst.u = src.u;
        dst.v = src.v;
    }
}

void __stdcall xrSkin1W(vertRender* D, vertBoned1W* S, u32 vCount, CBoneInstance* Bones) {
    if (vCount < 1024) {
        xrSkin1W_SSE_Internal(D, S, vCount, Bones);
        return;
    }
    tbb::task_group tg;
    u32 chunk_size = 1024;
    for (u32 i = 0; i < vCount; i += chunk_size) {
        tg.run([=] {
            u32 count = (i + chunk_size > vCount) ? (vCount - i) : chunk_size;
            xrSkin1W_SSE_Internal(D + i, S + i, count, Bones);
        });
    }
    tg.wait();
}

void xrSkin2W_SSE_Internal(vertRender* D, vertBoned2W* S, u32 vCount, CBoneInstance* Bones) {
    for (u32 i = 0; i < vCount; ++i) {
        const vertBoned2W& src = S[i];
        vertRender& dst = D[i];

        float w0 = src.w;
        float w1 = 1.0f - w0;

        const Fmatrix& M0 = Bones[src.matrix0].mRenderTransform;
        const Fmatrix& M1 = Bones[src.matrix1].mRenderTransform;

        __m128 P0 = TransformPoint(src.P, M0);
        __m128 P1 = TransformPoint(src.P, M1);
        
        __m128 P_sum = _mm_add_ps(_mm_mul_ps(P0, _mm_set1_ps(w0)), _mm_mul_ps(P1, _mm_set1_ps(w1)));

        __m128 N0 = TransformDir(src.N, M0);
        __m128 N1 = TransformDir(src.N, M1);
        
        __m128 N_sum = _mm_add_ps(_mm_mul_ps(N0, _mm_set1_ps(w0)), _mm_mul_ps(N1, _mm_set1_ps(w1)));

        _mm_storeu_ps((float*)&dst.P, P_sum);
        _mm_storeu_ps((float*)&dst.N, N_sum);
        
        dst.u = src.u;
        dst.v = src.v;
    }
}

void __stdcall xrSkin2W(vertRender* D, vertBoned2W* S, u32 vCount, CBoneInstance* Bones) {
    if (vCount < 1024) {
        xrSkin2W_SSE_Internal(D, S, vCount, Bones);
        return;
    }
    tbb::task_group tg;
    u32 chunk_size = 1024;
    for (u32 i = 0; i < vCount; i += chunk_size) {
        tg.run([=] {
            u32 count = (i + chunk_size > vCount) ? (vCount - i) : chunk_size;
            xrSkin2W_SSE_Internal(D + i, S + i, count, Bones);
        });
    }
    tg.wait();
}

void xrSkin3W_SSE_Internal(vertRender* D, vertBoned3W* S, u32 vCount, CBoneInstance* Bones) {
    for (u32 i = 0; i < vCount; ++i) {
        const vertBoned3W& src = S[i];
        vertRender& dst = D[i];

        float w0 = src.w[0];
        float w1 = src.w[1];
        float w2 = 1.0f - w0 - w1;

        const Fmatrix& M0 = Bones[src.m[0]].mRenderTransform;
        const Fmatrix& M1 = Bones[src.m[1]].mRenderTransform;
        const Fmatrix& M2 = Bones[src.m[2]].mRenderTransform;

        __m128 P0 = TransformPoint(src.P, M0);
        __m128 P1 = TransformPoint(src.P, M1);
        __m128 P2 = TransformPoint(src.P, M2);
        
        __m128 P_sum = _mm_mul_ps(P0, _mm_set1_ps(w0));
        P_sum = _mm_add_ps(P_sum, _mm_mul_ps(P1, _mm_set1_ps(w1)));
        P_sum = _mm_add_ps(P_sum, _mm_mul_ps(P2, _mm_set1_ps(w2)));

        __m128 N0 = TransformDir(src.N, M0);
        __m128 N1 = TransformDir(src.N, M1);
        __m128 N2 = TransformDir(src.N, M2);
        
        __m128 N_sum = _mm_mul_ps(N0, _mm_set1_ps(w0));
        N_sum = _mm_add_ps(N_sum, _mm_mul_ps(N1, _mm_set1_ps(w1)));
        N_sum = _mm_add_ps(N_sum, _mm_mul_ps(N2, _mm_set1_ps(w2)));

        _mm_storeu_ps((float*)&dst.P, P_sum);
        _mm_storeu_ps((float*)&dst.N, N_sum);
        
        dst.u = src.u;
        dst.v = src.v;
    }
}

void __stdcall xrSkin3W(vertRender* D, vertBoned3W* S, u32 vCount, CBoneInstance* Bones) {
    if (vCount < 1024) {
        xrSkin3W_SSE_Internal(D, S, vCount, Bones);
        return;
    }
    tbb::task_group tg;
    u32 chunk_size = 1024;
    for (u32 i = 0; i < vCount; i += chunk_size) {
        tg.run([=] {
            u32 count = (i + chunk_size > vCount) ? (vCount - i) : chunk_size;
            xrSkin3W_SSE_Internal(D + i, S + i, count, Bones);
        });
    }
    tg.wait();
}

void xrSkin4W_SSE_Internal(vertRender* D, vertBoned4W* S, u32 vCount, CBoneInstance* Bones) {
    for (u32 i = 0; i < vCount; ++i) {
        const vertBoned4W& src = S[i];
        vertRender& dst = D[i];

        float w0 = src.w[0];
        float w1 = src.w[1];
        float w2 = src.w[2];
        float w3 = 1.0f - w0 - w1 - w2;

        const Fmatrix& M0 = Bones[src.m[0]].mRenderTransform;
        const Fmatrix& M1 = Bones[src.m[1]].mRenderTransform;
        const Fmatrix& M2 = Bones[src.m[2]].mRenderTransform;
        const Fmatrix& M3 = Bones[src.m[3]].mRenderTransform;

        __m128 P0 = TransformPoint(src.P, M0);
        __m128 P1 = TransformPoint(src.P, M1);
        __m128 P2 = TransformPoint(src.P, M2);
        __m128 P3 = TransformPoint(src.P, M3);

        __m128 P_sum = _mm_mul_ps(P0, _mm_set1_ps(w0));
        P_sum = _mm_add_ps(P_sum, _mm_mul_ps(P1, _mm_set1_ps(w1)));
        P_sum = _mm_add_ps(P_sum, _mm_mul_ps(P2, _mm_set1_ps(w2)));
        P_sum = _mm_add_ps(P_sum, _mm_mul_ps(P3, _mm_set1_ps(w3)));

        __m128 N0 = TransformDir(src.N, M0);
        __m128 N1 = TransformDir(src.N, M1);
        __m128 N2 = TransformDir(src.N, M2);
        __m128 N3 = TransformDir(src.N, M3);

        __m128 N_sum = _mm_mul_ps(N0, _mm_set1_ps(w0));
        N_sum = _mm_add_ps(N_sum, _mm_mul_ps(N1, _mm_set1_ps(w1)));
        N_sum = _mm_add_ps(N_sum, _mm_mul_ps(N2, _mm_set1_ps(w2)));
        N_sum = _mm_add_ps(N_sum, _mm_mul_ps(N3, _mm_set1_ps(w3)));

        _mm_storeu_ps((float*)&dst.P, P_sum);
        _mm_storeu_ps((float*)&dst.N, N_sum);
        
        dst.u = src.u;
        dst.v = src.v;
    }
}

void __stdcall xrSkin4W(vertRender* D, vertBoned4W* S, u32 vCount, CBoneInstance* Bones) {
    if (vCount < 1024) {
        xrSkin4W_SSE_Internal(D, S, vCount, Bones);
        return;
    }
    // TBB Implementation
    tbb::task_group tg;
    u32 chunk_size = 1024;
    for (u32 i = 0; i < vCount; i += chunk_size) {
        tg.run([=] {
            u32 count = (i + chunk_size > vCount) ? (vCount - i) : chunk_size;
            xrSkin4W_SSE_Internal(D + i, S + i, count, Bones);
        });
    }
    tg.wait();
}
