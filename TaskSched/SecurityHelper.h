#pragma once

struct SecurityHelper abstract final {
	static bool IsRunningElevated();
	static bool RunElevated();
	static bool EnablePrivilege(PCWSTR priv, bool enable = true);
};
