package  {
	
	import flash.display.MovieClip;
	import flash.utils.getDefinitionByName;
	import flash.geom.Point;
	import flash.text.TextFormat;
	
	
	public class WidgetHud extends MovieClip {
		
		private static const min_health:Number = 0;
		private static var max_health:Number;
		private static var meter_max_width:Number;
		private static var width_increment:Number;
		
		// Reference to vanilla hud menu instance
		private var hud_menu:Object;

		// Set max condition and width increments
		public static function SetMaxHealth(p_max_health:Number) {
			max_health = p_max_health;
			width_increment = meter_max_width / max_health;
		}
		
		public function WidgetHud() {
			super();
			// Get vanilla hud_menu instance
			hud_menu = (getDefinitionByName("hudframework.HUDFramework") as Class).getInstance().getHUD();
			// meter's max width value is set to initial width
			meter_max_width = bar.meter.width;
			hud_menu.RightMeters_mc.AmmoCount_mc.AmmoLineInstance.alpha = 0;
			// Set CND text to use vanilla font style
			var text_format:TextFormat = new TextFormat();
			text_format.font = "$MAIN_Font_Bold";
			cnd_text.display_text.setTextFormat(text_format);
		}
		
		// Set meter width depending on weapon condition
		public function SetHealth(health:Number) {
			
			if (health < min_health)
				bar.meter.width = min_health;
			else if (health > max_health)
				bar.meter.width = max_health;
			else
				bar.meter.width = width_increment * health;
		}
		
		public function SetVisibility(visible_flag:Boolean) {
			this.visible = visible_flag;
		}
		
		public function SetProps(state:int, health:Number) {

			SetVisibility(true);
			SetHealth(health);

			if (!state) {
				// In PA
				var crosshair_base:Object = hud_menu.CenterGroup_mc.HUDCrosshair_mc.CrosshairBase_mc;
				crosshair_base.addChild(this);
				SetPosToAmmoLine();
				this.x = this.x - 160;
				this.y = this.y + 75;
			}
			else if (state == 1) {
				// Using Melee
				var compass_bar:Object = hud_menu.BottomCenterGroup_mc.CompassWidget_mc.CompassBar_mc;
				// Change parent of WidgetHud instance to CompassBar_mc element
				compass_bar.addChild(this);
				SetPosToAmmoLine();
			}
			else {
				// Using Gun
				var ammo_line:Object = hud_menu.RightMeters_mc.AmmoCount_mc.AmmoLineInstance;
				// Change parent of WidgetHud instance to AmmoCount element
				hud_menu.RightMeters_mc.AmmoCount_mc.addChild(this);
				// Set position to AmmoLineInstance
				this.x = ammo_line.x - ammo_line.width;
				this.y = ammo_line.y;
			}

		}
		
		private function SetPosToAmmoLine() {
			var ammo_line:Object = hud_menu.RightMeters_mc.AmmoCount_mc.AmmoLineInstance;
			var ammo_line_global:Point = ammo_line.localToGlobal(new Point(ammo_line.x, ammo_line.y));
			var new_pos:Point = this.parent.globalToLocal(new Point(ammo_line_global.x, ammo_line_global.y));
			this.x = new_pos.x - ammo_line.width;
			this.y = new_pos.y;
		}
	}
}
