using System;
using System.IO;

namespace UnrealBuildTool.Rules 
{
	public class RealSensePlugin : ModuleRules 
    {
	private string ModulePath
	{
		get { return Path.GetDirectoryName(RulesCompiler.GetModuleFilename(this.GetType().Name)); }
	}

	private string ThirdPartyPath
	{
		get {
            return Path.GetFullPath(Path.Combine(ModulePath, "../../Thirdparty/"));
            }
	}
        public RealSensePlugin(TargetInfo Target)
        {
            // https://answers.unrealengine.com/questions/51798/how-can-i-enable-unwind-semantics-for-c-style-exce.html
            UEBuildConfiguration.bForceEnableExceptions = true;

            PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", });
            PrivateDependencyModuleNames.AddRange(new string[] { "RHI", "RenderCore", "ShaderCore",			
					"ProceduralMeshComponent",
					"ImageCore",
					"Media",
					"MediaAssets",
					"RawAudio"
 });

            PrivateIncludePaths.AddRange(new string[] { "RealSensePlugin/Private" });

            string RealSenseDirectory = Environment.GetEnvironmentVariable("RSSDK_DIR");
            string RealSenseIncludeDirectory = RealSenseDirectory + "include";
            string RealSenseLibrary32Directory = RealSenseDirectory + "lib\\Win32\\libpxc.lib";
            string RealSenseLibrary64Directory = RealSenseDirectory + "lib\\x64\\libpxc.lib";

            PublicIncludePaths.Add(RealSenseIncludeDirectory);
            PublicAdditionalLibraries.Add(RealSenseLibrary32Directory);
            PublicAdditionalLibraries.Add(RealSenseLibrary64Directory);
            PrivateIncludePaths.Add(Path.Combine(ThirdPartyPath, "snappy-windows-1.1.1.8", "include"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "snappy-windows-1.1.1.8", "src", "x64", "Release", "Snappy-static-lib.lib"));
        }
	}
}