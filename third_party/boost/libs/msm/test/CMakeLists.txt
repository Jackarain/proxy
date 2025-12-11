if(BOOST_MSM_IS_ROOT)
    find_package(Boost REQUIRED COMPONENTS unit_test_framework)
endif()

if(NOT TARGET tests)
    add_custom_target(tests)
endif()

link_libraries(Boost::msm Boost::unit_test_framework)
add_compile_definitions("BOOST_MSM_NONSTANDALONE_TEST")
if(BOOST_MSM_TEST_ONLY_BACKMP11)
    add_compile_definitions("BOOST_MSM_TEST_ONLY_BACKMP11")
endif()
if(BOOST_MSM_TEST_STRICT)
    add_compile_options(-Wall -Wextra -Werror -Wno-language-extension-token)
endif()


add_executable(boost_msm_tests
    EXCLUDE_FROM_ALL
    AnonymousAndGuard.cpp
    Anonymous.cpp
    Back11CompositeMachine.cpp
    BigWithFunctors.cpp
    CompositeMachine.cpp
    Constructor.cpp
    Entries.cpp
    EventQueue.cpp
    History.cpp
    KleeneDeferred.cpp
    ManyDeferTransitions.cpp
    OrthogonalDeferred2.cpp
    OrthogonalDeferred3.cpp
    OrthogonalDeferred.cpp
    Serialize.cpp
    SerializeWithHistory.cpp
    SetStates.cpp
    SimpleInternal.cpp
    SimpleInternalFunctors.cpp
    SimpleKleene.cpp
    SimpleMachine.cpp
    SimpleWithFunctors.cpp
    Test2RegionsAnonymous.cpp
    TestConstructor.cpp
    TestConstructorMovableOnlyTypes.cpp
    TestDeferAndMessageQueue2.cpp
    TestDeferAndMessageQueue3.cpp
    TestDeferAndMessageQueue.cpp
    TestDeferIn2Regions.cpp
    Throwing.cpp
    TransitionSkipping.cpp
    main.cpp
)
add_test(NAME boost_msm_tests COMMAND boost_msm_tests)
add_dependencies(tests boost_msm_tests)

# Keep eUML tests in a separate target
# to reduce wait times in subset test runs.
add_executable(boost_msm_euml_tests
    EXCLUDE_FROM_ALL
    AnonymousEuml.cpp
    CompositeEuml.cpp
    OrthogonalDeferredEuml.cpp
    SerializeSimpleEuml.cpp
    SimpleEuml2.cpp
    SimpleEuml.cpp
    SimpleInternalEuml.cpp
    main.cpp
)
add_test(NAME boost_msm_euml_tests COMMAND boost_msm_euml_tests)
add_dependencies(tests boost_msm_euml_tests)

add_executable(boost_msm_cxx17_tests
    EXCLUDE_FROM_ALL
    Backmp11Constructor.cpp
    Backmp11Context.cpp
    Backmp11Deferred.cpp
    Backmp11ManyDeferTransitions.cpp
    Backmp11Members.cpp
    Backmp11RootSm.cpp
    Backmp11Visitor.cpp
    main.cpp
)
target_compile_features(boost_msm_cxx17_tests PRIVATE cxx_std_17)
if(((DEFINED CMAKE_CXX_STANDARD)     AND (CMAKE_CXX_STANDARD GREATER_EQUAL 17)) OR
   ((NOT DEFINED CMAKE_CXX_STANDARD) AND ("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES)))
    add_test(NAME boost_msm_cxx17_tests COMMAND boost_msm_cxx17_tests)
    add_dependencies(tests boost_msm_cxx17_tests)
endif()

add_executable(boost_msm_cxx20_tests
    EXCLUDE_FROM_ALL
    Back11OrthogonalDeferred3WithPuml.cpp
    OnlyStringPuml.cpp
    puml_syntax_2.cpp
    puml_syntax.cpp
    SimpleWithPuml.cpp
    SimpleInternalWithPuml.cpp
    StringTerminatePuml.cpp
    main.cpp
)
target_compile_features(boost_msm_cxx20_tests PRIVATE cxx_std_20)
if(((DEFINED CMAKE_CXX_STANDARD)     AND (CMAKE_CXX_STANDARD GREATER_EQUAL 20)) OR
   ((NOT DEFINED CMAKE_CXX_STANDARD) AND ("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)))
    add_test(NAME boost_msm_cxx20_tests COMMAND boost_msm_cxx20_tests)
    add_dependencies(tests boost_msm_cxx20_tests)
endif()
