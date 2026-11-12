-- The version names NEC.psc.in is filled with. xmake.lua's on_config gives
-- them to NEC.psc, and xmake/apidoc.lua to the API document, so the script
-- and the document always name the same version.

-- ${NEC_VERSION} and ${NEC_VERSION_NUMBER} for a_version, such as "1.9.1":
-- the text as given, and major times 10000 plus minor times 100 plus patch,
-- 10901 for 1.9.1. A minor or a patch past 99 would not fit that number,
-- so it stops. src/Papyrus/Version.cpp NumberOf must give the same number.
function configvars(a_version)
    local major, minor, patch = (a_version or ""):match("^(%d+)%.(%d+)%.(%d+)$")
    if not major or tonumber(minor) > 99 or tonumber(patch) > 99 then
        raise("the version \"%s\" is not major.minor.patch with a minor and a patch up to 99", a_version or "")
    end
    return {
        NEC_VERSION = a_version,
        NEC_VERSION_NUMBER = tonumber(major) * 10000 + tonumber(minor) * 100 + tonumber(patch),
    }
end
