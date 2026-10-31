// Per-skin embedded assets. Each Resources/skins/<folder> becomes its own
// binary-data library (see CMakeLists.txt), so a skin can be added or removed
// without touching code other than the list below.
#include "PhotorealLookAndFeel.h"

#if FV_HAS_SKIN_harmonizer79
 #include "FreddySkin_harmonizer79.h"
#endif
#if FV_HAS_SKIN_reverb84
 #include "FreddySkin_reverb84.h"
#endif
#if FV_HAS_SKIN_broadcast
 #include "FreddySkin_broadcast.h"
#endif
#if FV_HAS_SKIN_champagne
 #include "FreddySkin_champagne.h"
#endif
#if FV_HAS_SKIN_lab
 #include "FreddySkin_lab.h"
#endif

namespace fv::ui
{
namespace
{
    template <typename GetNamed, typename Names, typename Files>
    std::pair<const void*, int> lookup (GetNamed getNamed, Names names, Files files, int count, const juce::String& fileName)
    {
        for (int i = 0; i < count; ++i)
            if (fileName == files[i])
            {
                int size = 0;
                const char* data = getNamed (names[i], size);
                return { data, size };
            }
        return { nullptr, 0 };
    }
}

#define FV_SKIN_CASE(name) \
    if (folder == #name) \
        return lookup (FreddySkin_##name::getNamedResource, FreddySkin_##name::namedResourceList, \
                       FreddySkin_##name::originalFilenames, FreddySkin_##name::namedResourceListSize, file);

std::pair<const void*, int> SkinAssets::get (const juce::String& folder, const juce::String& file)
{
   #if FV_HAS_SKIN_harmonizer79
    FV_SKIN_CASE (harmonizer79)
   #endif
   #if FV_HAS_SKIN_reverb84
    FV_SKIN_CASE (reverb84)
   #endif
   #if FV_HAS_SKIN_broadcast
    FV_SKIN_CASE (broadcast)
   #endif
   #if FV_HAS_SKIN_champagne
    FV_SKIN_CASE (champagne)
   #endif
   #if FV_HAS_SKIN_lab
    FV_SKIN_CASE (lab)
   #endif
    juce::ignoreUnused (folder, file);
    return { nullptr, 0 };
}

bool SkinAssets::exists (const juce::String& folder)
{
    return get (folder, "layout.json").first != nullptr;
}
} // namespace fv::ui
