# M2 Technical & Design Understanding

This file is an assessed technical-understanding artifact, not ordinary project documentation.
Answer all four questions using your own submitted implementation. Concise answers are acceptable when they are technically correct and specific.

Generic descriptions of C++ concepts or restatements of the assignment that do not identify and explain corresponding parts of your code will receive limited credit.

## 1. Polymorphism and dynamic dispatch - 1.5 points

Identify one place in your M2 implementation where runtime polymorphism occurs. Name the relevant base interface, derived implementation, and `ProcessingCore` function involved. Trace the call from `ProcessingCore` to the selected strategy implementation and explain why the derived implementation is invoked.

Then explain what would change if the relevant operation were not declared `virtual`.


ProcessingCore::search() calls impl_->retrieval->search(...), where impl_->retrieval is a unique_ptr RetrievalStrategy pointing at whatever derived class was injected, like ReverseOrderRetrieval in the student tests. Because search() is virtual, that call goes through the vtable and runs ReverseOrderRetrieval's actual code instead of RetrievalStrategy's, even though search() itself never changed. If it weren't virtual, the call would just try to resolve to RetrievalStrategy's own version, which doesn't exist since it's pure virtual, so it wouldn't compile at all.





## 2. Ownership and lifetime - 1.5 points

Identify where one of the strategy objects is created, where ownership is transferred, and which object ultimately owns it. Explain how `std::unique_ptr` represents that ownership relationship and when the strategy object is destroyed.

Also explain why `ProcessingCore` is move-only and why the strategy base classes require virtual destructors.


The strategy object is created with make_unique in the test, then moved into the ProcessingCore constructor and again into Impl, so Impl ends up as the sole owner and the object is destroyed automatically when the ProcessingCore does. ProcessingCore is move-only because unique_ptr itself can't be copied, so a struct holding three of them can't either. The strategy base classes need virtual destructors because they're deleted through a base pointer, and without a virtual destructor that would only run the base class's cleanup and skip the derived class's.




## 3. Architecture, extensibility, and M1 compatibility - 1.5 points

Explain one specific architectural decision in your M2 implementation that makes the processing system extensible while preserving M1 behavior.

Identify the classes or interfaces involved and explain both:
- how the default configuration preserves M1 behavior; and
- how a different implementation can be substituted without changing the normal `ProcessingCore` API.

Include one plausible design alternative and explain why the M2 design is preferable for this milestone. The alternative does not need to be something you actually implemented.



The main change was making ProcessingCore::Impl hold unique_ptr ChunkingStrategy/RetrievalStrategy/ContextStrategy instead of concrete Chunker/RetrievalEngine/ContextBuilder members like M1 did. The default constructor still builds the same Chunker/RetrievalEngine/ContextBuilder as M1, just stored through their base pointers, so nothing about default behavior changes, and a different implementation can be swapped in through the second constructor without touching rebuild/search/build_context at all. A template-based alternative would pick the implementation at compile time instead, which would be faster but wouldn't allow choosing a strategy at runtime, which is the whole point of M2.



## 4. Testing and defect reasoning - 1.5 points

Select one meaningful test from your `tests/student_tests.cpp`.

Explain:
- what M2 requirement the test validates;
- what specific implementation defect the test could detect; and
- why your test provides useful evidence beyond simply rerunning the supplied public tests.

If your test uses a custom strategy, explain how its observable behavior demonstrates that `ProcessingCore` is actually using runtime substitution.


The "WholeDocumentChunking overrides default chunker" test runs the same 150-token document through a default core and a custom core and checks that the default makes 2 chunks while the custom makes 1. This specifically catches a bug where rebuild() still secretly calls a hardcoded Chunker instead of the injected impl_->chunking, since that bug would make the custom core also show 2 chunks. It's stronger than the public test because it forces the default chunker's own splitting logic to actually trigger first, so matching 1 chunk on the custom side can't happen by accident


