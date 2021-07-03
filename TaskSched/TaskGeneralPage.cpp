#include "pch.h"
#include "resource.h"
#include "TaskGeneralPage.h"

LRESULT CTaskGeneralPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    InitDynamicLayout(false);
    return LRESULT();
}
