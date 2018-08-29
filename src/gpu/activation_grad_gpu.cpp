/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "activation_grad_inst.h"
#include "primitive_gpu_base.h"
#include "implementation_map.h"
#include "error_handler.h"
#include "kernel_selector_helper.h"
#include "activation/activation_kernel_selector.h"
#include "activation/activation_kernel_base.h"
#include "api/CPP/activation_grad.hpp"

namespace cldnn { namespace gpu {


struct activation_grad_gpu : typed_primitive_gpu_impl<activation_grad>
{
    using parent = typed_primitive_gpu_impl<activation_grad>;
    using parent::parent;

    virtual kernel::kernel_arguments_data get_arguments(typed_primitive_inst<activation_grad>& instance, int32_t split) const override
    {
        kernel::kernel_arguments_data args = parent::get_arguments(instance, split);

        if (_outer.is_parameterized())
        {
            args.slope = &instance.slope_memory();
        }

        return args;
    }

    static primitive_impl* create(const activation_grad_node& arg) 
    { 
        auto activation_grad_params = get_default_params<kernel_selector::activation_params>(arg);
        auto activation_grad_optional_params = get_default_optional_params<kernel_selector::activation_optional_params>(arg.get_program());

        const auto& primitive = arg.get_primitive();

        activation_grad_params.gradient = true;
        activation_grad_params.inputs.push_back(convert_data_tensor(arg.get_dependency(1).get_output_layout()));
        activation_grad_params.activationFunc = get_kernel_selector_activation_grad_param(primitive->activation_grad_func);
        activation_grad_params.activationParams.m = primitive->additional_params.a;
        activation_grad_params.activationParams.n = primitive->additional_params.b;

        if (arg.is_parameterized())
        {
            const auto& slope_layout = arg.slope_input().get_output_layout();
            const auto& output_layout = arg.get_output_layout();

            const auto params_num = kernel_selector::GetActivationAdditionalParamsNumber(activation_grad_params.activationFunc);

            CLDNN_ERROR_LESS_THAN(arg.id(), "Slope layout size count", slope_layout.size.count(), "output_layout.size.feature[0] * params_num", static_cast<size_t>(output_layout.size.feature[0] * params_num), "Error - not enough data inside additional params buffer");

            activation_grad_params.inputActivationParams.push_back(convert_data_tensor(slope_layout));
        }

        auto& kernel_selector = kernel_selector::activation_kernel_selector::Instance();
        auto best_kernels = kernel_selector.GetBestKernels(activation_grad_params, activation_grad_optional_params);
        CLDNN_ERROR_BOOL(arg.id(), "Best_kernel.empty()", best_kernels.empty(), "Cannot find a proper kernel with this arguments");

        auto activation_grad = new activation_grad_gpu(arg, best_kernels[0]);

        return activation_grad;
    };
};


namespace {
    struct attach {
        attach() {
            auto val_fw = activation_grad_gpu::create;
    
            implementation_map<activation_grad>::add({
                { std::make_tuple(engine_types::ocl, data_types::f32, format::yxfb), val_fw},
                { std::make_tuple(engine_types::ocl, data_types::f16, format::yxfb), val_fw},
                { std::make_tuple(engine_types::ocl, data_types::f32, format::bfyx), val_fw },
                { std::make_tuple(engine_types::ocl, data_types::f16, format::bfyx), val_fw },
                { std::make_tuple(engine_types::ocl, data_types::f32, format::byxf), val_fw },
                { std::make_tuple(engine_types::ocl, data_types::f16, format::byxf), val_fw },
            });
        }
        ~attach() {}
    };
    attach attach_impl;
}
} }
