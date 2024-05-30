/* Copyright 2024 The OpenXLA Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "xla/service/gpu/gemm_degenerate_dim_remover.h"

#include <optional>

#include <gtest/gtest.h>
#include "xla/tests/hlo_test_base.h"

namespace xla::gpu {
namespace {

class GemmDegenerateDimRemoverTest : public HloTestBase {};

TEST_F(GemmDegenerateDimRemoverTest, RewriteMatrixVectorMultiplicationToGemm) {
  const char* hlo = R"(
  HloModule m

  ENTRY e {
    p0 = f32[32,7] parameter(0)
    p1 = f32[7] parameter(1)
    bitcast = f32[7, 1] bitcast(p1)
    dot = f32[32,1] dot(p0, bitcast),
      lhs_contracting_dims={1}, rhs_contracting_dims={0}
    ROOT result = f32[32] bitcast(dot)
  })";

  const char* expected = R"()
// CHECK:  %[[P0:.*]] = f32[32,7]{1,0} parameter(0)
// CHECK:  %[[P1:.*]] = f32[7]{0} parameter(1)
// CHECK:  %[[BROADCAST:.*]] = f32[32,7]{1,0} broadcast(%[[P1]])
// CHECK:  %[[MULTIPLY:.*]] = f32[32,7]{1,0} multiply(%[[P0]], %[[BROADCAST]])
// CHECK:  %[[CONSTANT:.*]] = f32[] constant(0)
// CHECK:  ROOT %[[REDUCE:.*]] = f32[32]{0} reduce(%[[MULTIPLY]], %[[CONSTANT]]), dimensions={1}
})";

  RunAndFilecheckHloRewrite(hlo, GemmDegenerateDimRemover(), expected);
}

TEST_F(GemmDegenerateDimRemoverTest, RewriteVectorMatrixMultiplicationToGemm) {
  const char* hlo = R"(
  HloModule m

  ENTRY e {
    p0 = f32[7] parameter(0)
    p1 = f32[32,7] parameter(1)
    bitcast = f32[7, 1] bitcast(p0)
    dot = f32[1,32] dot(bitcast, p1),
      lhs_contracting_dims={0}, rhs_contracting_dims={1}
    ROOT result = f32[32] bitcast(dot)
  })";

  const char* expected = R"()
// CHECK:  %[[P0:.*]] = f32[7]{0} parameter(0)
// CHECK:  %[[BROADCAST:.*]] = f32[32,7]{1,0} broadcast(%[[P0]])
// CHECK:  %[[P1:.*]] = f32[32,7]{1,0} parameter(1)
// CHECK:  %[[MULTIPLY:.*]] = f32[32,7]{1,0} multiply(%[[BROADCAST]], %[[P1]])
// CHECK:  %[[CONSTANT:.*]] = f32[] constant(0)
// CHECK:  ROOT %[[REDUCE:.*]] = f32[32]{0} reduce(%[[MULTIPLY]], %[[CONSTANT]]), dimensions={1}
})";

  RunAndFilecheckHloRewrite(hlo, GemmDegenerateDimRemover(), expected);
}

TEST_F(GemmDegenerateDimRemoverTest,
       RewriteMatrixVectorMultiplicationWithBatch) {
  const char* hlo = R"(
  HloModule m

  ENTRY e {
    p0 = f32[2,5,32,7] parameter(0)
    p1 = f32[2,5,7] parameter(1)
    bitcast  = f32[2,5,7,1]{3,2,1,0} bitcast(p1)
    d = f32[2,5,32,1] dot(p0, bitcast),
      lhs_batch_dims={0,1}, rhs_batch_dims={0,1},
      lhs_contracting_dims={3}, rhs_contracting_dims={2}
    ROOT result = f32[2,5,32] bitcast(d)
  })";

  const char* expected = R"()
// CHECK:  %[[P0:.*]] = f32[2,5,32,7]{3,2,1,0} parameter(0)
// CHECK:  %[[P1:.*]] = f32[2,5,7]{2,1,0} parameter(1)
// CHECK:  %[[BROADCAST:.*]] = f32[2,5,32,7]{3,2,1,0} broadcast(%[[P1]])
// CHECK:  %[[MULTIPLY:.*]] = f32[2,5,32,7]{3,2,1,0} multiply(%[[P0]], %[[BROADCAST]])
// CHECK:  %[[CONSTANT:.*]] = f32[] constant(0)
// CHECK:  ROOT %[[REDUCE:.*]] = f32[2,5,32]{2,1,0} reduce(%[[MULTIPLY]], %[[CONSTANT]]), dimensions={3}
})";

  RunAndFilecheckHloRewrite(hlo, GemmDegenerateDimRemover(), expected);
}

TEST_F(GemmDegenerateDimRemoverTest,
       DoNotRewriteDegenerateDimNotInsertedByGemvRewriter) {
  const char* hlo = R"(
  HloModule m

  ENTRY e {
    p0 = f32[32,7] parameter(0)
    p1 = f32[7, 1] parameter(1)
    dot = f32[32,1] dot(p0, p1),
      lhs_contracting_dims={1}, rhs_contracting_dims={0}
    ROOT result = f32[32] bitcast(dot)
  })";

  RunAndFilecheckHloRewrite(hlo, GemmDegenerateDimRemover(),
                            /*expected=*/std::nullopt);
}

}  // namespace
}  // namespace xla::gpu
