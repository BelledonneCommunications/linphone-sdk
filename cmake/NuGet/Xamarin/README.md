# Linphone SDK NuGet Package for Xamarin Development

The `LinphoneSDK.Xamarin` folder is a Visual Studio solution that wraps the Linphone SDK to provide .NET bindings for Android & iOS cross-platform development with Xamarin.

It corresponds to stage 2 & 3 of the "four-stage approach used to transform C/C++ source code into a cross-platform Xamarin library that is shared via NuGet and then is consumed in a Xamarin.Forms app" as described in [this Microsoft documentation](https://docs.microsoft.com/en-us/xamarin/cross-platform/cpp/#high-level-approach).

The output is a single `.nuget` that embeds the Linphone SDK, natively compiled for the required architectures.

For build instructions, please refer to the parent folder's [README](../README.md), the [`CMakeLists.txt`](CMakeLists.txt) and the `job-xamarin-package` CI job.
