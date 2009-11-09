/* layer_stack_test.cc
   Jeremy Barnes, 9 November2009
   Copyright (c) 2009 Jeremy Barnes.  All rights reserved.

   Unit tests for the layer stack class.
*/


#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#undef NDEBUG

#include <boost/test/unit_test.hpp>
#include <boost/multi_array.hpp>
#include "neural/dense_layer.h"
#include "neural/layer_stack.h"
#include "utils/testing/serialize_reconstitute_include.h"
#include <boost/assign/list_of.hpp>
#include <limits>

using namespace ML;
using namespace ML::DB;
using namespace std;

using boost::unit_test::test_suite;

BOOST_AUTO_TEST_CASE( test_serialize_reconstitute_dense_layer0a )
{
    Thread_Context context;
    int ni = 2, no = 4;
    Dense_Layer<float> layer("test", ni, no, TF_TANH, MV_ZERO, context);

    // Test equality operator
    BOOST_CHECK_EQUAL(layer, layer);

    Dense_Layer<float> layer2 = layer;
    BOOST_CHECK_EQUAL(layer, layer2);
    
    layer2.weights[0][0] -= 1.0;
    BOOST_CHECK(layer != layer2);

    BOOST_CHECK_EQUAL(layer.weights.shape()[0], ni);
    BOOST_CHECK_EQUAL(layer.weights.shape()[1], no);
    BOOST_CHECK_EQUAL(layer.bias.size(), no);
    BOOST_CHECK_EQUAL(layer.missing_replacements.size(), 0);
    BOOST_CHECK_EQUAL(layer.missing_activations.num_elements(), 0);
    
    test_serialize_reconstitute(layer);
}

BOOST_AUTO_TEST_CASE( test_serialize_reconstitute_dense_layer0b )
{
    Thread_Context context;
    Dense_Layer<float> layer("test", 20, 40, TF_TANH, MV_INPUT, context);
    BOOST_CHECK_EQUAL(layer.weights.shape()[0], 20);
    BOOST_CHECK_EQUAL(layer.weights.shape()[1], 40);
    BOOST_CHECK_EQUAL(layer.bias.size(), 40);
    BOOST_CHECK_EQUAL(layer.missing_replacements.size(), 20);
    BOOST_CHECK_EQUAL(layer.missing_activations.num_elements(), 0);
    test_serialize_reconstitute(layer);
}

BOOST_AUTO_TEST_CASE( test_serialize_reconstitute_dense_layer0c )
{
    Thread_Context context;
    Dense_Layer<float> layer("test", 20, 40, TF_IDENTITY, MV_DENSE, context);
    BOOST_CHECK_EQUAL(layer.weights.shape()[0], 20);
    BOOST_CHECK_EQUAL(layer.weights.shape()[1], 40);
    BOOST_CHECK_EQUAL(layer.bias.size(), 40);
    BOOST_CHECK_EQUAL(layer.missing_replacements.size(), 0);
    BOOST_CHECK_EQUAL(layer.missing_activations.num_elements(), 800);
    test_serialize_reconstitute(layer);
}

BOOST_AUTO_TEST_CASE( test_serialize_reconstitute_dense_layer0d )
{
    Thread_Context context;
    Dense_Layer<float> layer("test", 20, 40, TF_LOGSIG, MV_NONE, context);
    BOOST_CHECK_EQUAL(layer.weights.shape()[0], 20);
    BOOST_CHECK_EQUAL(layer.weights.shape()[1], 40);
    BOOST_CHECK_EQUAL(layer.bias.size(), 40);
    BOOST_CHECK_EQUAL(layer.missing_replacements.size(), 0);
    BOOST_CHECK_EQUAL(layer.missing_activations.num_elements(), 0);
    test_serialize_reconstitute(layer);
}

BOOST_AUTO_TEST_CASE( test_dense_layer_none )
{
    Dense_Layer<float> layer("test", 2, 1, TF_IDENTITY, MV_NONE);
    layer.weights[0][0] = 0.5;
    layer.weights[1][0] = 2.0;
    layer.bias[0] = 0.0;

    distribution<float> input
        = boost::assign::list_of<float>(1.0)(-1.0);

    // Check that the basic functions work
    BOOST_REQUIRE_EQUAL(layer.apply(input).size(), 1);
    BOOST_CHECK_EQUAL(layer.apply(input)[0], -1.5);

    // Check the missing values throw an exception
    input[0] = numeric_limits<float>::quiet_NaN();
    BOOST_CHECK_THROW(layer.apply(input), ML::Exception);

    // Check that the wrong size throws an exception
    input.push_back(2.0);
    input[0] = 1.0;
    BOOST_CHECK_THROW(layer.apply(input), ML::Exception);

    input.pop_back();

    // Check that the bias works
    layer.bias[0] = 1.0;
    BOOST_CHECK_EQUAL(layer.apply(input)[0], -0.5);

    // Check that there are parameters
    BOOST_CHECK_EQUAL(layer.parameters().parameter_count(), 3);

    // Check the info
    BOOST_CHECK_EQUAL(layer.inputs(), 2);
    BOOST_CHECK_EQUAL(layer.outputs(), 1);
    BOOST_CHECK_EQUAL(layer.name(), "test");

    // Check the copy constructor
    Dense_Layer<float> layer2 = layer;
    BOOST_CHECK_EQUAL(layer2, layer);
    BOOST_CHECK_EQUAL(layer2.parameters().parameter_count(), 3);

    // Check the assignment operator
    Dense_Layer<float> layer3;
    BOOST_CHECK(layer3 != layer);
    layer3 = layer;
    BOOST_CHECK_EQUAL(layer3, layer);
    BOOST_CHECK_EQUAL(layer3.parameters().parameter_count(), 3);

    // Make sure that the assignment operator didn't keep a reference
    layer3.weights[0][0] = 5.0;
    BOOST_CHECK_EQUAL(layer.weights[0][0], 0.5);
    BOOST_CHECK_EQUAL(layer3.weights[0][0], 5.0);
    layer3.weights[0][0] = 0.5;
    BOOST_CHECK_EQUAL(layer, layer3);

    // Check fprop (that it gives the same result as apply)
    distribution<float> applied
        = layer.apply(input);

    size_t temp_space_size = layer.fprop_temporary_space_required();

    float temp_space[temp_space_size];

    distribution<float> fproped
        = layer.fprop(input, temp_space, temp_space_size);

    BOOST_CHECK_EQUAL_COLLECTIONS(applied.begin(), applied.end(),
                                  fproped.begin(), fproped.end());

    // Check parameters
    Parameters_Copy<float> params(layer.parameters());
    distribution<float> & param_dist = params.values;

    BOOST_CHECK_EQUAL(param_dist.size(), 3);
    BOOST_CHECK_EQUAL(param_dist.at(0), 0.5);  // weight 0
    BOOST_CHECK_EQUAL(param_dist.at(1), 2.0);  // weight 1
    BOOST_CHECK_EQUAL(param_dist.at(2), 1.0);  // bias

    Thread_Context context;
    layer3.random_fill(-1.0, context);

    BOOST_CHECK(layer != layer3);

    layer3.parameters().set(params);
    
    BOOST_CHECK_EQUAL(layer, layer3);

    // Check backprop
    distribution<float> output_errors(1, 1.0);
    distribution<float> input_errors;
    Parameters_Copy<float> gradient(layer.parameters());
    gradient.fill(0.0);
    layer.bprop(output_errors, temp_space, temp_space_size,
                gradient, input_errors, 1.0, true /* calculate_input_errors */);

    BOOST_CHECK_EQUAL(input_errors.size(), layer.inputs());

    // Check the values of input errors.  It's easy since there's only one
    // weight that contributes to each input (since there's only one output).
    BOOST_CHECK_EQUAL(input_errors[0], layer.weights[0][0]);
    BOOST_CHECK_EQUAL(input_errors[1], layer.weights[1][0]);

    //cerr << "input_errors = " << input_errors << endl;

    // Check that example_weight scales the gradient
    Parameters_Copy<float> gradient2(layer.parameters());
    gradient2.fill(0.0);
    layer.bprop(output_errors, temp_space, temp_space_size,
                gradient2, input_errors, 2.0,
                true /* calculate_input_errors */);

    //cerr << "gradient.values = " << gradient.values << endl;
    //cerr << "gradient2.values = " << gradient2.values << endl;
    
    distribution<float> gradient_times_2 = gradient.values * 2.0;
    
    BOOST_CHECK_EQUAL_COLLECTIONS(gradient2.values.begin(),
                                  gradient2.values.end(),
                                  gradient_times_2.begin(),
                                  gradient_times_2.end());
}

BOOST_AUTO_TEST_CASE( test_serialize_reconstitute_dense_layer1 )
{
    Thread_Context context;
    Dense_Layer<float> layer("test", 200, 400, TF_TANH, MV_ZERO, context);
    test_serialize_reconstitute(layer);
}

BOOST_AUTO_TEST_CASE( test_serialize_reconstitute_dense_layer2 )
{
    Thread_Context context;
    Dense_Layer<float> layer("test", 200, 400, TF_TANH, MV_INPUT, context);
    test_serialize_reconstitute(layer);
}

BOOST_AUTO_TEST_CASE( test_serialize_reconstitute_dense_layer3 )
{
    Thread_Context context;
    Dense_Layer<float> layer("test", 200, 400, TF_TANH, MV_DENSE, context);
    test_serialize_reconstitute(layer);
}

BOOST_AUTO_TEST_CASE( test_serialize_reconstitute_dense_layer_double )
{
    Thread_Context context;
    Dense_Layer<double> layer("test", 200, 400, TF_TANH, MV_DENSE, context);
    test_serialize_reconstitute(layer);
}
