/*
Copyright 2013-present Barefoot Networks, Inc. 

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

// Function calls with template parameters are parsed with the wrong priority.
// This is a bug in the bison grammar which is hard to fix.
// The workaround is to use parentheses.

extern T f<T>(T x);

action a()
{
    bit<32> x;
    x = (bit<32>)f<bit<6> >(6w5);
}

bit<32> f(inout bit x, in bit b)
{
	bit<32> y;
}
