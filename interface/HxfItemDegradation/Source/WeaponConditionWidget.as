package {

	import flash.display.MovieClip;
	import hudframework.IHUDWidget;

	public class WeaponConditionWidget extends MovieClip implements IHUDWidget {

		private static const WIDGET_IDENTIFIER:String = "HxfItemDegradation/hxfID_WeaponConditionWidget.swf";
		private static const Command_SetMaxHealth:String = '1';
		private static const Command_SetHealth:String = '2';
		private static const Command_SetVisibility:String = '3';
		private static const Command_SetProps:String = '4';
		
		// Reference to the component that contains all the condition bar elements
		public var widget_hud:WidgetHud;
		
		public function WeaponConditionWidget() {
			super();
		}
      
		public function processMessage(param1:String, param2:Array) : void {

			var health:Number;
			var visible_flag:Boolean;
			
			switch(param1) {
				
				case Command_SetMaxHealth:
					var max_health:Number = Number(param2[0]);
					WidgetHud.SetMaxHealth(max_health);
					break;

				case Command_SetHealth:
					health = Number(param2[0]);
					widget_hud.SetHealth(health);
					break;
				
				case Command_SetVisibility:
					visible_flag = Boolean(Number(param2[0]));
					widget_hud.SetVisibility(visible_flag);
					break;
				
				case Command_SetProps:
					var state:int = int(param2[0]);
					health = Number(param2[1]);
					widget_hud.SetProps(state, health);
					break;
			}
		}
	}

}