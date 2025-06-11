-- https://github.com/Arcthesia/ArcCreate/wiki/Grid-settings

local spacing = {
    x = 0.1,
    y = 0.1,
}

local color = {
    dark = rgba(0, 0, 0, 50),
    light = rgba(150, 150, 150, 50),
    panel = rgba(255, 255, 255, 16),
}

local config = {
    left = -0.2,
    bottom = 0.0,

    xCount = 14,
    yCount = 360,
    yHint = 4,
}

config.right = config.left + config.xCount * spacing.x
config.top = config.bottom + config.yCount * spacing.y

grid.setCollider(config.left, config.right, config.bottom, config.top)
grid.setPanelColor(color.panel)

grid.drawLine(config.left, config.left, config.bottom, config.top, color.light)
grid.drawLine(config.right, config.right, config.bottom, config.top, color.light)
grid.drawLine(config.left, config.right, config.top, config.top, color.light)
grid.drawLine(config.left, config.right, config.bottom, config.bottom, color.light)

for i = 0, config.xCount do
    local x = i * spacing.x + config.left
    local col = color.light
    grid.drawLine(x, x, config.bottom, config.top, col)
end

for i = 0, config.yCount do
    local y = i * spacing.y + config.bottom
    local col = color.light
    if i % config.yHint == 0 then
        col = color.dark
    end
    grid.drawLine(config.left, config.right, y, y, col)
end
