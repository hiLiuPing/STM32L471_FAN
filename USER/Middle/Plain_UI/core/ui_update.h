/*
 * 每帧更新入口。
 * UI_Update_Init 负责注册图层回调。
 * UI_Update_RunFrame 负责模型同步、动画推进和统一绘制。
 */
#ifndef UI_UPDATE_H
#define UI_UPDATE_H

void UI_Update_Init(void);
void UI_Update_RunFrame(void);

#endif /* UI_UPDATE_H */
