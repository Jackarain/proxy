if(BOOST_MSM_IS_ROOT)
    find_package(Boost REQUIRED COMPONENTS unit_test_framework)
endif()

if(NOT TARGET tests)
    add_custom_target(tests)
endif()

link_libraries(Boost::msm Boost::unit_test_framework)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../doc/modules/ROOT)
add_compile_definitions("BOOST_MSM_NONSTANDALONE_TEST")
if(BOOST_MSM_TEST_ONLY_BACKMP11)
    add_compile_definitions("BOOST_MSM_TEST_ONLY_BACKMP11")
endif()
if(BOOST_MSM_TEST_STRICT)
    add_compile_options(-Wall -Wpedantic -Wextra -Wconversion -Werror -Wno-language-extension-token)
endif()
if(BOOST_MSM_TEST_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fno-omit-frame-pointer)
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
    OrthogonalDeferred.cpp
    OrthogonalDeferred2.cpp
    OrthogonalDeferred3.cpp
    OrthogonalDeferred4.cpp
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
    Backmp11BasicPolymorphic.cpp
    Backmp11Completion.cpp
    Backmp11Constructor.cpp
    Backmp11Context.cpp
    Backmp11CopyMove.cpp
    Backmp11Deferred.cpp
    Backmp11EntryExit.cpp
    Backmp11Exceptions.cpp
    Backmp11FunctorApi.cpp
    Backmp11Heapless.cpp
    Backmp11History.cpp
    Backmp11MachineState.cpp
    Backmp11ManyDeferTransitions.cpp
    Backmp11Observer.cpp
    Backmp11RootSm.cpp
    Backmp11Serialization.cpp
    Backmp11StateMachineInterface.cpp
    Backmp11Transitions.cpp
    Backmp11Visitor.cpp
    main.cpp
)
if(NOT BOOST_MSM_IS_ROOT)
    target_link_libraries(boost_msm_cxx17_tests Boost::json)
endif()
if(BOOST_MSM_TEST_NLOHMANN_JSON)
    target_compile_definitions(boost_msm_cxx17_tests PRIVATE "BOOST_MSM_TEST_NLOHMANN_JSON")
endif()
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
