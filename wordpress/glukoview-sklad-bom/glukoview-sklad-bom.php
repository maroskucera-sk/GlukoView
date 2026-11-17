<?php
/**
 * Plugin Name: GlukoView Sklad BOM
 * Plugin URI: https://github.com/maroskucera-sk/GlukoView
 * Description: Predaj zloženého zariadenia (produkt B, napr. GlukoView) odpočíta fyzický sklad komponentu (produkt A, napr. doska ESP32). Na webe zariadenie ukazuje presne ten istý stav skladu ako komponent. Jeden kus zariadenia spotrebuje jeden kus komponentu.
 * Version: 1.0.0
 * Author: GlukoView
 * Requires at least: 6.2
 * Requires PHP: 7.4
 * Requires Plugins: woocommerce
 * WC requires at least: 8.2
 * WC tested up to: 10.3
 * Text Domain: glukoview-sklad-bom
 *
 * Inštalácia: priečinok glukoview-sklad-bom skopírujte do wp-content/plugins/
 * a plugin aktivujte. Potom WooCommerce → Sklad BOM.
 *
 * @package GlukoView_Sklad_Bom
 */

defined( 'ABSPATH' ) || exit;

define( 'GV_BOM_FILE', __FILE__ );
define( 'GV_BOM_VERSION', '1.0.0' );

/**
 * WooCommerce 8+ inak pri vlastných tabuľkách objednávok (HPOS) vypíše varovanie.
 * Hook sa musí zavesiť pri načítaní súboru, nie až na plugins_loaded.
 */
add_action( 'before_woocommerce_init', 'gv_bom_declare_woocommerce_compatibility' );

/**
 * @return void
 */
function gv_bom_declare_woocommerce_compatibility() {
	if ( ! class_exists( '\Automattic\WooCommerce\Utilities\FeaturesUtil' ) ) {
		return;
	}

	\Automattic\WooCommerce\Utilities\FeaturesUtil::declare_compatibility( 'custom_order_tables', GV_BOM_FILE, true );
	\Automattic\WooCommerce\Utilities\FeaturesUtil::declare_compatibility( 'cart_checkout_blocks', GV_BOM_FILE, true );
}

add_action( 'plugins_loaded', 'gv_bom_bootstrap', 20 );

/**
 * @return void
 */
function gv_bom_bootstrap() {
	if ( ! class_exists( 'WooCommerce' ) ) {
		add_action( 'admin_notices', 'gv_bom_woocommerce_missing_notice' );
		return;
	}

	GlukoView_Sklad_Bom::instance()->register_hooks();
}

/**
 * @return void
 */
function gv_bom_woocommerce_missing_notice() {
	if ( ! current_user_can( 'activate_plugins' ) ) {
		return;
	}

	echo '<div class="notice notice-error"><p>GlukoView Sklad BOM potrebuje aktívny WooCommerce.</p></div>';
}

/**
 * Skladová väzba 1:1 medzi komponentom a zloženým zariadením.
 *
 * Produkt B nemá vlastnú skladovú zásobu. Všetky kusy žijú na produkte A.
 * Natívna správa skladu produktu B je preto vypnutá: WooCommerce by inak
 * pri objednávke znížil aj _stock produktu B a sklad by sa odpočítal dvakrát.
 */
final class GlukoView_Sklad_Bom {

	const OPTION = 'gv_bom_settings';
	const LEDGER = '_gv_bom_reduced_qty';

	/**
	 * Filter pre vývojára. Predvolene 1 — jeden kus zariadenia berie jeden kus komponentu.
	 * add_filter( 'glukoview_bom_kusov_komponentu', fn() => 1 );
	 */
	const RATIO_FILTER = 'glukoview_bom_kusov_komponentu';

	/** @var self|null */
	private static $instance = null;

	/** @var array<string,int>|null Dočasné nastavenie počas sanitizácie, kým ho WordPress nezapíše. */
	private $settings_override = null;

	/** @var bool Ochrana pred zacyklením, keď pri zrkadlení čítame sklad komponentu. */
	private $mirroring = false;

	/** @var bool Ochrana pred zacyklením pri ukladaní stavu skladu zariadenia. */
	private $syncing = false;

	/** @var bool */
	private $component_loaded = false;

	/** @var WC_Product|null */
	private $component_product = null;

	/** @var bool */
	private $assembled_loaded = false;

	/** @var WC_Product|null */
	private $assembled_product = null;

	/** @var array<int,bool> */
	private $variable_cache = array();

	/**
	 * @return self
	 */
	public static function instance() {
		if ( null === self::$instance ) {
			self::$instance = new self();
		}

		return self::$instance;
	}

	/**
	 * @return void
	 */
	public function register_hooks() {
		add_action( 'admin_menu', array( $this, 'register_menu' ) );
		add_action( 'admin_init', array( $this, 'register_settings' ) );
		add_filter( 'option_page_capability_gv_bom_group', array( $this, 'settings_capability' ) );

		// Predaj a storno. WooCommerce tieto akcie spustí aj keď produkt B sklad nespravuje:
		// wc_reduce_stock_levels() / wc_increase_stock_levels() ich volajú na konci vždy.
		add_action( 'woocommerce_reduce_order_stock', array( $this, 'reduce_component_stock' ) );
		add_action( 'woocommerce_restore_order_stock', array( $this, 'restore_component_stock' ) );

		// Refund so zaškrtnutým „vrátiť do skladu“ nejde cez woocommerce_restore_order_stock.
		// Ten hook je len pre storno / zlyhanie / návrat do stavu čaká na platbu.
		add_action( 'woocommerce_refund_created', array( $this, 'restore_component_stock_on_refund' ), 10, 2 );

		// Výklad: reálne filtre WooCommerce. Aliasy nižšie sú tie názvy zo zadania
		// (jadro ich samo nevolá, ale vlastný kód obchodu áno).
		foreach ( array( 'woocommerce_product_get_stock_quantity', 'woocommerce_product_variation_get_stock_quantity', 'woocommerce_get_stock_quantity' ) as $hook ) {
			add_filter( $hook, array( $this, 'filter_stock_quantity' ), 10, 2 );
		}
		foreach ( array( 'woocommerce_product_get_stock_status', 'woocommerce_product_variation_get_stock_status' ) as $hook ) {
			add_filter( $hook, array( $this, 'filter_stock_status' ), 10, 2 );
		}
		add_filter( 'woocommerce_product_is_in_stock', array( $this, 'filter_is_in_stock' ), 10, 2 );
		add_filter( 'woocommerce_is_in_stock', array( $this, 'filter_is_in_stock' ), 10, 2 );
		add_filter( 'woocommerce_get_availability', array( $this, 'filter_availability' ), 10, 2 );

		// Pohľad na produkt B nikdy nespravuje sklad, ani keď ho niekto v karte produktu zapne.
		// Kontext edit (karta produktu) filter nedostane, preto sklad vypíname aj pri uložení.
		add_filter( 'woocommerce_product_get_manage_stock', array( $this, 'filter_manage_stock' ), 10, 2 );
		add_filter( 'woocommerce_product_variation_get_manage_stock', array( $this, 'filter_manage_stock' ), 10, 2 );
		add_filter( 'woocommerce_prevent_adjust_line_item_product_stock', array( $this, 'prevent_line_stock_adjust' ), 10, 3 );
		add_action( 'woocommerce_admin_process_product_object', array( $this, 'force_assembled_unmanaged' ) );
		add_action( 'woocommerce_product_quick_edit_save', array( $this, 'force_assembled_unmanaged' ) );
		add_action( 'woocommerce_product_bulk_edit_save', array( $this, 'force_assembled_unmanaged' ) );

		add_action( 'woocommerce_product_set_stock', array( $this, 'on_component_stock_object' ) );
		add_action( 'woocommerce_variation_set_stock', array( $this, 'on_component_stock_object' ) );
		add_action( 'woocommerce_product_set_stock_status', array( $this, 'on_component_stock_status' ), 10, 3 );

		add_filter( 'woocommerce_add_to_cart_validation', array( $this, 'validate_add_to_cart' ), 10, 5 );
		add_filter( 'woocommerce_update_cart_validation', array( $this, 'validate_cart_update' ), 10, 4 );
		add_action( 'woocommerce_check_cart_items', array( $this, 'validate_cart_stock' ) );
		add_action( 'woocommerce_store_api_cart_errors', array( $this, 'validate_store_api_cart' ), 10, 2 );
		add_action( 'woocommerce_store_api_validate_add_to_cart', array( $this, 'validate_store_api_add' ), 10, 2 );
		add_filter( 'woocommerce_quantity_input_args', array( $this, 'filter_quantity_args' ), 10, 2 );
		add_filter( 'woocommerce_store_api_product_quantity_maximum', array( $this, 'filter_store_api_maximum' ), 10, 2 );

		// Rezervácia drží kusy komponentu, kým je objednávka nezaplatená.
		// Klasická pokladňa ide cez woocommerce_checkout_order_created (až po natívnej rezervácii).
		// Pokladňa s blokmi volá wc_reserve_stock_for_order() priamo a tento hook nespustí.
		add_action( 'woocommerce_checkout_order_created', array( $this, 'reserve_component_for_order' ), 20 );
		add_action( 'woocommerce_store_api_checkout_order_processed', array( $this, 'reserve_component_for_order' ), 5 );
	}

	/**
	 * @return string
	 */
	public function settings_capability() {
		return 'manage_woocommerce';
	}

	/**
	 * @return void
	 */
	public function register_menu() {
		add_submenu_page(
			'woocommerce',
			'Sklad BOM',
			'Sklad BOM',
			'manage_woocommerce',
			'glukoview-sklad-bom',
			array( $this, 'render_page' )
		);
	}

	/**
	 * @return void
	 */
	public function register_settings() {
		register_setting(
			'gv_bom_group',
			self::OPTION,
			array(
				'type'              => 'array',
				'sanitize_callback' => array( $this, 'sanitize_settings' ),
				'default'           => array(
					'component_id' => 0,
					'assembled_id' => 0,
				),
				'show_in_rest'      => false,
			)
		);

		add_settings_section(
			'gv_bom_main',
			'Prepojenie produktov',
			array( $this, 'render_section_intro' ),
			'glukoview-sklad-bom'
		);

		add_settings_field(
			'component_id',
			'Komponent (produkt A)',
			array( $this, 'render_id_field' ),
			'glukoview-sklad-bom',
			'gv_bom_main',
			array(
				'key'         => 'component_id',
				'description' => 'Fyzický diel so zapnutou správou skladu. ID je číslo v adrese úpravy produktu (post=123 alebo produkt s týmto ID).',
			)
		);

		add_settings_field(
			'assembled_id',
			'Zložené zariadenie (produkt B)',
			array( $this, 'render_id_field' ),
			'glukoview-sklad-bom',
			'gv_bom_main',
			array(
				'key'         => 'assembled_id',
				'description' => 'Hotové zariadenie. Pri uložení sa mu vypne správa skladu, aby WooCommerce neodpočítaval jeho vlastné kusy.',
			)
		);
	}

	/**
	 * @param mixed $input Surové odoslané pole.
	 * @return array{component_id:int,assembled_id:int}
	 */
	public function sanitize_settings( $input ) {
		$input     = is_array( $input ) ? $input : array();
		$component = isset( $input['component_id'] ) ? absint( $input['component_id'] ) : 0;
		$assembled = isset( $input['assembled_id'] ) ? absint( $input['assembled_id'] ) : 0;

		if ( $component && ! wc_get_product( $component ) ) {
			add_settings_error( self::OPTION, 'gv_bom_component_missing', 'Komponent s týmto ID neexistuje.', 'error' );
			$component = 0;
		}

		if ( $assembled && ! wc_get_product( $assembled ) ) {
			add_settings_error( self::OPTION, 'gv_bom_assembled_missing', 'Zariadenie s týmto ID neexistuje.', 'error' );
			$assembled = 0;
		}

		if ( $component && $assembled && $component === $assembled ) {
			add_settings_error( self::OPTION, 'gv_bom_same', 'Komponent a zariadenie musia byť dva rôzne produkty.', 'error' );
			$assembled = 0;
		}

		$component_product = $component ? wc_get_product( $component ) : null;
		if ( $component_product && ! $component_product->managing_stock() ) {
			add_settings_error( self::OPTION, 'gv_bom_component_stock', 'Komponent nemá zapnutú správu skladu. Zapnite ju na produkte A, inak plugin nemá čo odpočítavať.', 'error' );
		}

		$clean = array(
			'component_id' => $component,
			'assembled_id' => $assembled,
		);

		$this->settings_override = $clean;
		$this->flush_product_cache();

		if ( $assembled ) {
			$this->disable_native_stock( $assembled );
		}
		if ( $component && $assembled ) {
			$this->sync_assembled_stock_status();
		}

		$this->settings_override = null;
		$this->flush_product_cache();

		return $clean;
	}

	/**
	 * @return void
	 */
	public function render_section_intro() {
		echo '<p>Jeden predaný kus zariadenia zníži sklad komponentu o <strong>1 kus</strong>. ';
		echo 'Dostupnosť zariadenia na webe, v košíku aj v pokladni sa berie zo skladu komponentu. ';
		echo 'Keď komponent dôjde, zariadenie sa ukáže ako vypredané.</p>';
		echo '<p>V WooCommerce musí byť zapnutá globálna správa skladu (WooCommerce → Nastavenia → Produkty → Sklad). ';
		echo 'Bez nej sa odpočet pri objednávke vôbec nespustí.</p>';
	}

	/**
	 * @param array{key:string,description:string} $args Argumenty poľa.
	 * @return void
	 */
	public function render_id_field( $args ) {
		$key      = $args['key'];
		$settings = $this->settings();
		$value    = isset( $settings[ $key ] ) ? (int) $settings[ $key ] : 0;
		$name     = self::OPTION . '[' . $key . ']';

		printf(
			'<input type="number" class="small-text" min="0" step="1" id="%1$s" name="%2$s" value="%3$d" />',
			esc_attr( $key ),
			esc_attr( $name ),
			$value
		);

		echo '<p class="description">' . esc_html( $args['description'] ) . '</p>';

		if ( $value ) {
			$product = wc_get_product( $value );
			if ( $product ) {
				$link = get_edit_post_link( $product->get_id() );
				echo '<p>';
				if ( $link ) {
					printf( '<a href="%s">%s</a>', esc_url( $link ), esc_html( $product->get_formatted_name() ) );
				} else {
					echo esc_html( $product->get_formatted_name() );
				}
				echo '</p>';
			}
		}
	}

	/**
	 * @return void
	 */
	public function render_page() {
		if ( ! current_user_can( 'manage_woocommerce' ) ) {
			return;
		}

		echo '<div class="wrap">';
		echo '<h1>Sklad BOM</h1>';
		settings_errors();
		$this->render_status();

		echo '<form action="options.php" method="post">';
		settings_fields( 'gv_bom_group' );
		do_settings_sections( 'glukoview-sklad-bom' );
		submit_button( 'Uložiť' );
		echo '</form>';
		echo '</div>';
	}

	/**
	 * @return void
	 */
	private function render_status() {
		if ( 'yes' !== get_option( 'woocommerce_manage_stock' ) ) {
			echo '<div class="notice notice-warning inline"><p>Globálna správa skladu WooCommerce je vypnutá. Odpočet komponentu sa pri objednávke nespustí.</p></div>';
		}

		if ( ! $this->ready() ) {
			return;
		}

		$component = $this->component();
		$assembled = $this->assembled();
		if ( ! $component || ! $assembled ) {
			return;
		}

		$qty = $component->managing_stock() ? wc_format_stock_quantity_for_display( $component->get_stock_quantity(), $component ) : 'nespravuje sa';

		echo '<table class="widefat striped" style="max-width:720px;margin:12px 0 20px"><tbody>';
		printf(
			'<tr><th>Komponent</th><td>%s</td></tr>',
			esc_html( $component->get_formatted_name() )
		);
		printf( '<tr><th>Sklad komponentu</th><td>%s (%s)</td></tr>', esc_html( (string) $qty ), esc_html( wc_get_stock_status_name( $component->get_stock_status() ) ) );
		printf( '<tr><th>Zariadenie</th><td>%s</td></tr>', esc_html( $assembled->get_formatted_name() ) );
		printf(
			'<tr><th>Vlastný sklad zariadenia</th><td>%s</td></tr>',
			$assembled->get_manage_stock( 'edit' ) ? 'zapnutý — pri ďalšom uložení produktu sa vypne' : 'vypnutý'
		);
		printf(
			'<tr><th>Stav na webe</th><td>%s</td></tr>',
			esc_html( wc_get_stock_status_name( $component->get_stock_status() ) )
		);
		echo '</tbody></table>';
	}

	/**
	 * Predaj zariadenia zníži sklad komponentu o kúpené množstvo.
	 * Účet na objednávke bráni druhému priechodu toho istého hooku.
	 *
	 * @param WC_Order|int $order Objednávka.
	 * @return void
	 */
	public function reduce_component_stock( $order ) {
		$order = $this->as_order( $order );
		if ( ! $order || ! $this->ready() ) {
			return;
		}

		$target  = $this->bom_units_for_quantity( $this->units_in_order( $order, 'assembled' ) );
		$already = $this->ledger( $order );
		$delta   = (float) wc_stock_amount( $target - $already );

		if ( $delta <= 0 ) {
			return;
		}

		if ( ! $this->move_component_stock( $order, $delta, 'decrease' ) ) {
			return;
		}

		$this->set_ledger( $order, $already + $delta );
	}

	/**
	 * Storno, zlyhaná platba alebo návrat do „čaká na platbu“ vráti ešte neodvrátené kusy.
	 *
	 * @param WC_Order|int $order Objednávka.
	 * @return void
	 */
	public function restore_component_stock( $order ) {
		$order = $this->as_order( $order );
		if ( ! $order || ! $this->ready() ) {
			return;
		}

		$already = $this->ledger( $order );
		if ( $already <= 0 ) {
			return;
		}

		if ( ! $this->move_component_stock( $order, $already, 'increase' ) ) {
			return;
		}

		$this->set_ledger( $order, 0 );
	}

	/**
	 * Čiastočný alebo úplný refund so zaškrtnutým vrátením položiek do skladu.
	 * Bez toho zaškrtnutia sa kusy nevracajú — rovnako ako pri bežnom produkte.
	 *
	 * @param int   $refund_id ID refundu.
	 * @param array $args      Argumenty wc_create_refund(), vrátane restock_items a line_items.
	 * @return void
	 */
	public function restore_component_stock_on_refund( $refund_id, $args = array() ) {
		if ( empty( $args['restock_items'] ) || empty( $args['line_items'] ) || ! is_array( $args['line_items'] ) || ! $this->ready() ) {
			return;
		}

		$refund = wc_get_order( $refund_id );
		if ( ! $refund || ! is_callable( array( $refund, 'get_parent_id' ) ) ) {
			return;
		}

		$order = wc_get_order( $refund->get_parent_id() );
		if ( ! $order instanceof WC_Order ) {
			return;
		}

		$units = 0.0;
		foreach ( $args['line_items'] as $item_id => $data ) {
			$item = $order->get_item( $item_id );
			if ( ! $item || ! $this->item_matches( $item, 'assembled' ) ) {
				continue;
			}
			$qty = ( is_array( $data ) && isset( $data['qty'] ) ) ? (float) $data['qty'] : 0.0;
			if ( $qty > 0 ) {
				$units += $qty;
			}
		}

		$restore = (float) wc_stock_amount( min( $this->bom_units_for_quantity( $units ), $this->ledger( $order ) ) );
		if ( $restore <= 0 ) {
			return;
		}

		if ( ! $this->move_component_stock( $order, $restore, 'increase' ) ) {
			return;
		}

		$this->set_ledger( $order, $this->ledger( $order ) - $restore );
	}

	/**
	 * @param mixed           $quantity Pôvodné množstvo.
	 * @param WC_Product|null $product  Produkt, ak ho filter dostal.
	 * @return mixed
	 */
	public function filter_stock_quantity( $quantity, $product = null ) {
		if ( $this->mirroring || ! $product instanceof WC_Product || ! $this->is_assembled_product( $product ) ) {
			return $quantity;
		}

		$component = $this->read_component();
		if ( ! $component ) {
			return $quantity;
		}

		return $component->get_stock_quantity();
	}

	/**
	 * @param mixed           $status  Pôvodný stav.
	 * @param WC_Product|null $product Produkt.
	 * @return mixed
	 */
	public function filter_stock_status( $status, $product = null ) {
		if ( $this->mirroring || ! $product instanceof WC_Product || ! $this->is_assembled_product( $product ) ) {
			return $status;
		}

		$component = $this->read_component();
		if ( ! $component ) {
			return 'outofstock';
		}

		return $component->get_stock_status();
	}

	/**
	 * @param bool            $in_stock Pôvodná dostupnosť.
	 * @param WC_Product|null $product  Produkt.
	 * @return bool
	 */
	public function filter_is_in_stock( $in_stock, $product = null ) {
		if ( $this->mirroring || ! $product instanceof WC_Product || ! $this->is_assembled_product( $product ) ) {
			return $in_stock;
		}

		$component = $this->read_component();
		if ( ! $component ) {
			return false;
		}

		return $component->is_in_stock();
	}

	/**
	 * Text dostupnosti (vrátane počtu kusov) sa preberie z komponentu,
	 * takže rešpektuje jazyk obchodu aj formát zobrazenia skladu.
	 *
	 * @param array           $availability Pôvodná dostupnosť.
	 * @param WC_Product|null $product      Produkt.
	 * @return array
	 */
	public function filter_availability( $availability, $product = null ) {
		if ( $this->mirroring || ! $product instanceof WC_Product || ! $this->is_assembled_product( $product ) ) {
			return $availability;
		}

		$component = $this->read_component();
		if ( ! $component || $component->get_id() === $product->get_id() ) {
			return $availability;
		}

		$mirrored = $component->get_availability();

		return is_array( $mirrored ) ? $mirrored : $availability;
	}

	/**
	 * @param mixed           $manage  Uložená hodnota.
	 * @param WC_Product|null $product Produkt.
	 * @return mixed
	 */
	public function filter_manage_stock( $manage, $product = null ) {
		if ( ! $product instanceof WC_Product || ! $this->is_assembled_product( $product ) ) {
			return $manage;
		}

		return false;
	}

	/**
	 * Úprava množstva v už uloženej objednávke nesmie hýbať _stock produktu B.
	 *
	 * @param bool           $prevent Či WooCommerce úpravu preskočí.
	 * @param WC_Order_Item  $item    Položka.
	 * @param int            $qty     Množstvo. Nepoužíva sa, podpis filtra ho vyžaduje.
	 * @return bool
	 */
	public function prevent_line_stock_adjust( $prevent, $item = null, $qty = 0 ) {
		unset( $qty );

		if ( $prevent || ! $item || ! is_callable( array( $item, 'get_product_id' ) ) ) {
			return $prevent;
		}

		return $this->item_matches( $item, 'assembled' ) ? true : $prevent;
	}

	/**
	 * @param WC_Product $product Ukladaný produkt.
	 * @return void
	 */
	public function force_assembled_unmanaged( $product ) {
		if ( ! $product instanceof WC_Product || ! $this->is_assembled_product( $product ) || ! $product->get_manage_stock( 'edit' ) ) {
			return;
		}

		$product->set_manage_stock( false );

		// Rýchla a hromadná úprava volajú tento hook až po $product->save().
		// Karta produktu ukladá objekt až po woocommerce_admin_process_product_object.
		$action = current_action();
		if ( 'woocommerce_product_quick_edit_save' === $action || 'woocommerce_product_bulk_edit_save' === $action ) {
			$product->save();
		}
	}

	/**
	 * @param WC_Product $product Produkt, ktorému sa zmenil sklad.
	 * @return void
	 */
	public function on_component_stock_object( $product ) {
		if ( ! $product instanceof WC_Product || ! $this->is_component_stock_owner( $product ) ) {
			return;
		}

		$this->flush_product_cache();
		$this->sync_assembled_stock_status();
	}

	/**
	 * @param int             $product_id ID produktu.
	 * @param string          $status     Nový stav. Nepoužíva sa, stav sa číta z komponentu.
	 * @param WC_Product|null $product    Produkt.
	 * @return void
	 */
	public function on_component_stock_status( $product_id, $status = '', $product = null ) {
		unset( $status );

		$id = $product instanceof WC_Product ? $product->get_id() : $product_id;
		if ( ! $this->is_component_id( $id ) ) {
			return;
		}

		$this->flush_product_cache();
		$this->sync_assembled_stock_status();
	}

	/**
	 * @param bool  $passed       Doterajší výsledok.
	 * @param int   $product_id   ID produktu.
	 * @param mixed $quantity     Pridávané množstvo.
	 * @param int   $variation_id ID variácie.
	 * @param array $variations   Atribúty. Nepoužívajú sa.
	 * @return bool
	 */
	public function validate_add_to_cart( $passed, $product_id = 0, $quantity = 0, $variation_id = 0, $variations = array() ) {
		unset( $variations );

		if ( ! $passed ) {
			return false;
		}

		$error = $this->stock_error_for_addition( (int) $product_id, (int) $variation_id, (float) $quantity, null );
		if ( ! $error ) {
			return true;
		}

		wc_add_notice( $error, 'error' );
		return false;
	}

	/**
	 * @param bool   $passed        Doterajší výsledok.
	 * @param string $cart_item_key Kľúč položky.
	 * @param array  $values        Položka košíka.
	 * @param mixed  $quantity      Nové množstvo.
	 * @return bool
	 */
	public function validate_cart_update( $passed, $cart_item_key = '', $values = array(), $quantity = 0 ) {
		if ( ! $passed ) {
			return false;
		}

		$product_id   = isset( $values['product_id'] ) ? (int) $values['product_id'] : 0;
		$variation_id = isset( $values['variation_id'] ) ? (int) $values['variation_id'] : 0;
		$error        = $this->stock_error_for_addition( $product_id, $variation_id, (float) $quantity, (string) $cart_item_key );

		if ( ! $error ) {
			return true;
		}

		wc_add_notice( $error, 'error' );
		return false;
	}

	/**
	 * @return void
	 */
	public function validate_cart_stock() {
		$error = $this->stock_error_for_cart();
		if ( $error ) {
			wc_add_notice( $error, 'error' );
		}
	}

	/**
	 * @param WP_Error $errors Chyby Store API.
	 * @param WC_Cart  $cart   Košík.
	 * @return void
	 */
	public function validate_store_api_cart( $errors, $cart = null ) {
		unset( $cart );

		if ( ! $errors instanceof WP_Error ) {
			return;
		}

		$message = $this->stock_error_for_cart();
		if ( $message ) {
			$errors->add( 'gv_bom_stock', $message );
		}
	}

	/**
	 * @param WC_Product $product Pridávaný produkt.
	 * @param array      $request Požiadavka Store API.
	 * @return void
	 * @throws Exception Keď komponent nestačí.
	 */
	public function validate_store_api_add( $product, $request ) {
		if ( ! $product instanceof WC_Product ) {
			return;
		}

		$quantity = isset( $request['quantity'] ) ? (float) $request['quantity'] : 0;
		$error    = $this->stock_error_for_addition( (int) $product->get_parent_id() ? (int) $product->get_parent_id() : $product->get_id(), $product->is_type( 'variation' ) ? $product->get_id() : 0, $quantity, null );

		if ( ! $error ) {
			return;
		}

		if ( class_exists( '\Automattic\WooCommerce\StoreApi\Exceptions\RouteException' ) ) {
			throw new \Automattic\WooCommerce\StoreApi\Exceptions\RouteException( 'gv_bom_stock', $error, 400 );
		}

		throw new Exception( $error );
	}

	/**
	 * @param array      $args    Argumenty poľa množstva.
	 * @param WC_Product $product Produkt.
	 * @return array
	 */
	public function filter_quantity_args( $args, $product ) {
		if ( ! $product instanceof WC_Product || ! $this->is_assembled_product( $product ) ) {
			return $args;
		}

		$max = $this->max_assembled_quantity();
		if ( null === $max ) {
			return $args;
		}

		$args['max_value'] = $max;
		return $args;
	}

	/**
	 * @param mixed      $maximum Doterajšie maximum.
	 * @param WC_Product $product Produkt.
	 * @return mixed
	 */
	public function filter_store_api_maximum( $maximum, $product ) {
		if ( ! $product instanceof WC_Product || ! $this->is_assembled_product( $product ) ) {
			return $maximum;
		}

		$max = $this->max_assembled_quantity();
		if ( null === $max ) {
			return $maximum;
		}

		return min( (float) $maximum, $max );
	}

	/**
	 * Drží súčet priameho predaja komponentu a kusov spotrebovaných zariadením.
	 * WooCommerce medzitým (priorita 10) zapísal len priamy predaj. Tu riadok
	 * prepíšeme na celý fyzický odber tejto objednávky. Uvoľnenie rezervácie
	 * rieši jadro, lebo maže všetky riadky objednávky naraz.
	 *
	 * @param WC_Order $order Objednávka.
	 * @return void
	 * @throws Exception Keď rezervácia neprejde.
	 */
	public function reserve_component_for_order( $order ) {
		if ( ! $order instanceof WC_Order || ! $this->ready() ) {
			return;
		}

		if ( ! apply_filters( 'woocommerce_hold_stock_for_checkout', wc_string_to_bool( get_option( 'woocommerce_manage_stock', 'yes' ) ) ) ) {
			return;
		}

		$minutes = (int) apply_filters( 'woocommerce_order_hold_stock_minutes', (int) get_option( 'woocommerce_hold_stock_minutes', 60 ), $order );
		if ( $minutes <= 0 ) {
			return;
		}

		$component = $this->component();
		if ( ! $component || ! $component->managing_stock() || $component->backorders_allowed() ) {
			return;
		}

		$bom = $this->bom_units_for_quantity( $this->units_in_order( $order, 'assembled' ) );
		if ( $bom <= 0 ) {
			return;
		}

		$total      = (float) wc_stock_amount( $this->direct_component_units( $order ) + $bom );
		$stock_id   = (int) $component->get_stock_managed_by_id();
		$ok         = $this->upsert_reservation( $order->get_id(), $stock_id, $total, $minutes );
		if ( $ok ) {
			return;
		}

		$this->throw_reservation_failure(
			sprintf( 'Na objednávku nie je dosť kusov „%s“.', $component->get_name() )
		);
	}

	/**
	 * Klasická pokladňa čaká ReserveStockException. Bloky čakajú RouteException,
	 * inak by z nedostatku skladu vznikla chyba 500.
	 *
	 * @param string $message Text pre zákazníka.
	 * @return void
	 * @throws Exception Vždy.
	 */
	private function throw_reservation_failure( $message ) {
		if ( 'woocommerce_store_api_checkout_order_processed' === current_action() && class_exists( '\Automattic\WooCommerce\StoreApi\Exceptions\RouteException' ) ) {
			throw new \Automattic\WooCommerce\StoreApi\Exceptions\RouteException( 'woocommerce_product_not_enough_stock', $message, 403 );
		}

		if ( class_exists( '\Automattic\WooCommerce\Checkout\Helpers\ReserveStockException' ) ) {
			throw new \Automattic\WooCommerce\Checkout\Helpers\ReserveStockException( 'woocommerce_product_not_enough_stock', $message, 403 );
		}

		throw new Exception( $message );
	}

	/**
	 * @param int   $order_id  ID objednávky.
	 * @param int   $stock_id  ID produktu, ktorý drží _stock.
	 * @param float $total     Cieľové rezervované množstvo tejto objednávky.
	 * @param int   $minutes   Platnosť rezervácie.
	 * @return bool
	 */
	private function upsert_reservation( $order_id, $stock_id, $total, $minutes ) {
		global $wpdb;

		if ( $stock_id <= 0 || $total <= 0 ) {
			return false;
		}

		// Chýbajúca tabuľka nie je „nie je skladom“. Objednávku nenecháme spadnúť,
		// rezerváciu len preskočíme. Samotný odpočet pri platbe ostáva.
		if ( empty( $wpdb->wc_reserved_stock ) || ! class_exists( 'WC_Data_Store' ) ) {
			$this->log( 'Rezervácia komponentu sa preskočila: tabuľka rezervácií nie je k dispozícii.' );
			return true;
		}

		$data_store = WC_Data_Store::load( 'product' );
		if ( ! is_callable( array( $data_store, 'get_query_for_stock' ) ) ) {
			$this->log( 'Rezervácia komponentu sa preskočila: úložisko produktu nevie dotaz na sklad.' );
			return true;
		}

		$query_for_stock = str_replace( '%', '%%', $data_store->get_query_for_stock( $stock_id ) );
		$reserved_query  = str_replace( '%', '%%', $this->reserved_by_others_sql( $stock_id, $order_id ) );
		$table           = $wpdb->wc_reserved_stock;

		// Rovnaký zámok ako ReserveStock vo WooCommerce: sklad mínus cudzie rezervácie
		// musí pokryť celý fyzický odber tejto objednávky.
		$sql = $wpdb->prepare(
			"INSERT INTO {$table} ( `order_id`, `product_id`, `stock_quantity`, `timestamp`, `expires` )
			SELECT %d, %d, %f, NOW(), ( NOW() + INTERVAL %d MINUTE ) FROM DUAL
			WHERE ( {$query_for_stock} FOR UPDATE ) - ( {$reserved_query} LOCK IN SHARE MODE ) >= %f
			ON DUPLICATE KEY UPDATE `expires` = VALUES( `expires` ), `stock_quantity` = VALUES( `stock_quantity` )",
			$order_id,
			$stock_id,
			$total,
			$minutes,
			$total
		);

		for ( $attempt = 0; $attempt < 3; $attempt++ ) {
			$result = $wpdb->query( $sql ); // phpcs:ignore WordPress.DB.PreparedSQL.NotPrepared -- skladové dotazy sú už pripravené cez $wpdb->prepare a get_query_for_stock().
			if ( false !== $result ) {
				break;
			}
		}

		// Chyba dotazu (zlá schéma, deadlock po troch pokusoch) nie je vypredanie.
		// Odpočet pri zaplatení ostáva. Zákazníka kvôli tomu nezastavíme.
		if ( '' !== $wpdb->last_error ) {
			$this->log( 'Rezervácia komponentu zlyhala: ' . $wpdb->last_error );
			return true;
		}

		$stored = $wpdb->get_var(
			$wpdb->prepare(
				"SELECT stock_quantity FROM {$table} WHERE order_id = %d AND product_id = %d",
				$order_id,
				$stock_id
			)
		);

		return null !== $stored && abs( (float) $stored - (float) $total ) < 0.0001;
	}

	/**
	 * @param int $product_id       ID skladovej karty.
	 * @param int $exclude_order_id Objednávka, ktorej vlastná rezervácia sa neráta.
	 * @return string
	 */
	private function reserved_by_others_sql( $product_id, $exclude_order_id ) {
		global $wpdb;

		$hpos = class_exists( '\Automattic\WooCommerce\Utilities\OrderUtil' )
			&& \Automattic\WooCommerce\Utilities\OrderUtil::custom_orders_table_usage_is_enabled();

		if ( $hpos ) {
			$join   = "{$wpdb->prefix}wc_orders orders ON stock_table.order_id = orders.id";
			$status = "orders.status IN ( 'wc-checkout-draft', 'wc-pending' )";
		} else {
			$join   = "{$wpdb->posts} posts ON stock_table.order_id = posts.ID";
			$status = "posts.post_status IN ( 'wc-checkout-draft', 'wc-pending' )";
		}

		return $wpdb->prepare(
			"SELECT COALESCE( SUM( stock_table.stock_quantity ), 0 ) FROM {$wpdb->wc_reserved_stock} stock_table
			LEFT JOIN {$join}
			WHERE {$status}
			AND stock_table.expires > NOW()
			AND stock_table.product_id = %d
			AND stock_table.order_id != %d",
			$product_id,
			$exclude_order_id
		);
	}

	/**
	 * @param WC_Order $order     Objednávka, do ktorej sa píše poznámka.
	 * @param float    $amount    Pohyb v kusoch komponentu.
	 * @param string   $operation decrease alebo increase.
	 * @return bool
	 */
	private function move_component_stock( WC_Order $order, $amount, $operation ) {
		$component = $this->component();
		if ( ! $component ) {
			$order->add_order_note( 'Sklad BOM: komponent sa nenašiel, sklad sa nezmenil.' );
			$this->log( 'Komponent pre objednávku ' . $order->get_id() . ' neexistuje.' );
			return false;
		}

		if ( ! $component->managing_stock() ) {
			$order->add_order_note( sprintf( 'Sklad BOM: „%s“ nemá zapnutú správu skladu, stav sa nezmenil.', $component->get_name() ) );
			return false;
		}

		$amount = (float) wc_stock_amount( $amount );
		if ( $amount <= 0 ) {
			return false;
		}

		$new_stock = wc_update_product_stock( $component, $amount, $operation );
		if ( false === $new_stock || null === $new_stock || is_wp_error( $new_stock ) ) {
			$detail = is_wp_error( $new_stock ) ? $new_stock->get_error_message() : 'WooCommerce sklad nevrátil nový stav.';
			$order->add_order_note( sprintf( 'Sklad BOM: sklad „%s“ sa nepodarilo upraviť. %s', $component->get_name(), $detail ) );
			$this->log( $detail );
			return false;
		}

		$verb = 'decrease' === $operation ? 'znížený' : 'vrátený';
		$order->add_order_note(
			sprintf(
				'Sklad BOM: „%s“ %s o %s ks. Nový stav: %s.',
				$component->get_name(),
				$verb,
				wc_format_stock_quantity_for_display( $amount, $component ),
				wc_format_stock_quantity_for_display( $new_stock, $component )
			)
		);

		$this->flush_product_cache();
		return true;
	}

	/**
	 * @return void
	 */
	private function sync_assembled_stock_status() {
		if ( $this->syncing || ! $this->ready() ) {
			return;
		}

		$component = $this->component();
		$assembled = $this->assembled();
		if ( ! $component || ! $assembled ) {
			return;
		}

		$status  = $component->get_stock_status( 'edit' );
		$changed = false;

		if ( $assembled->get_stock_status( 'edit' ) !== $status ) {
			$assembled->set_stock_status( $status );
			$changed = true;
		}

		if ( $assembled->get_manage_stock( 'edit' ) ) {
			$assembled->set_manage_stock( false );
			$changed = true;
		}

		if ( ! $changed ) {
			return;
		}

		$this->syncing = true;
		$assembled->save();
		$this->syncing = false;
		$this->assembled_loaded = false;
		$this->assembled_product = null;
	}

	/**
	 * @param int $product_id ID zariadenia.
	 * @return void
	 */
	private function disable_native_stock( $product_id ) {
		$product = wc_get_product( $product_id );
		if ( ! $product || ! $product->get_manage_stock( 'edit' ) ) {
			return;
		}

		$product->set_manage_stock( false );
		$product->save();
		$this->assembled_loaded  = false;
		$this->assembled_product = null;
	}

	/**
	 * @param int    $product_id   ID produktu.
	 * @param int    $variation_id ID variácie.
	 * @param float  $quantity     Množstvo tejto operácie.
	 * @param string|null $replace_key Kľúč košíka, ktorého staré množstvo sa nahrádza.
	 * @return string|null Text chyby.
	 */
	private function stock_error_for_addition( $product_id, $variation_id, $quantity, $replace_key ) {
		if ( ! $this->ready() ) {
			return null;
		}

		$added = $this->demand_for_ids( $product_id, $variation_id, $quantity );
		if ( $added <= 0 ) {
			return null;
		}

		$available = $this->component_available( $this->session_order_id() );
		if ( null === $available ) {
			return null;
		}

		$demand = $this->cart_component_demand( $replace_key ) + $added;
		if ( $demand <= $available + 0.0001 ) {
			return null;
		}

		return $this->not_enough_message( $available );
	}

	/**
	 * @return string|null
	 */
	private function stock_error_for_cart() {
		if ( ! $this->ready() ) {
			return null;
		}

		$demand = $this->cart_component_demand( null );
		if ( $demand <= 0 ) {
			return null;
		}

		$available = $this->component_available( $this->session_order_id() );
		if ( null === $available || $demand <= $available + 0.0001 ) {
			return null;
		}

		return $this->not_enough_message( $available );
	}

	/**
	 * @param float $available Voľné kusy komponentu.
	 * @return string
	 */
	private function not_enough_message( $available ) {
		$component = $this->component();
		$name      = $component ? $component->get_name() : 'komponentu';
		$shown     = $component ? wc_format_stock_quantity_for_display( max( 0, $available ), $component ) : (string) max( 0, $available );

		return sprintf( 'Nie je dosť kusov „%s“. Voľných je %s.', $name, $shown );
	}

	/**
	 * @return float|null Null znamená, že limit neurčujeme (predobjednávky alebo sklad sa nespravuje).
	 */
	private function max_assembled_quantity() {
		$available = $this->component_available( $this->session_order_id() );
		if ( null === $available ) {
			return null;
		}

		$ratio = $this->ratio();
		if ( $ratio <= 0 ) {
			return 0.0;
		}

		return (float) wc_stock_amount( max( 0, $available / $ratio ) );
	}

	/**
	 * @param int $exclude_order_id Rezervácia tejto objednávky sa neráta (vlastný pokus o platbu).
	 * @return float|null
	 */
	private function component_available( $exclude_order_id ) {
		$component = $this->component();
		if ( ! $component || ! $component->managing_stock() ) {
			return null;
		}

		if ( $component->backorders_allowed() ) {
			return null;
		}

		if ( ! $component->is_in_stock() ) {
			return 0.0;
		}

		$held = function_exists( 'wc_get_held_stock_quantity' ) ? (float) wc_get_held_stock_quantity( $component, $exclude_order_id ) : 0.0;

		return max( 0.0, (float) $component->get_stock_quantity() - $held );
	}

	/**
	 * @param string|null $exclude_key Kľúč, ktorý sa z košíka vynechá.
	 * @return float
	 */
	private function cart_component_demand( $exclude_key ) {
		$cart = $this->cart();
		if ( ! $cart ) {
			return 0.0;
		}

		$demand = 0.0;
		foreach ( $cart->get_cart() as $key => $item ) {
			if ( null !== $exclude_key && (string) $key === (string) $exclude_key ) {
				continue;
			}

			$product_id   = isset( $item['product_id'] ) ? (int) $item['product_id'] : 0;
			$variation_id = isset( $item['variation_id'] ) ? (int) $item['variation_id'] : 0;
			$quantity     = isset( $item['quantity'] ) ? (float) $item['quantity'] : 0.0;
			$product      = ( isset( $item['data'] ) && $item['data'] instanceof WC_Product ) ? $item['data'] : null;
			$demand      += $this->demand_for_ids( $product_id, $variation_id, $quantity, $product );
		}

		return (float) wc_stock_amount( $demand );
	}

	/**
	 * @param int             $product_id   ID produktu.
	 * @param int             $variation_id ID variácie.
	 * @param float           $quantity     Množstvo.
	 * @param WC_Product|null $product      Objekt, ak už je k dispozícii.
	 * @return float Spotreba komponentu.
	 */
	private function demand_for_ids( $product_id, $variation_id, $quantity, $product = null ) {
		$quantity = (float) $quantity;
		if ( $quantity <= 0 || ! $this->ready() ) {
			return 0.0;
		}

		if ( $this->ids_match( $product_id, $variation_id, $this->component_id(), $product ) ) {
			return (float) wc_stock_amount( $quantity );
		}

		if ( $this->ids_match( $product_id, $variation_id, $this->assembled_id(), $product ) ) {
			return $this->bom_units_for_quantity( $quantity );
		}

		return 0.0;
	}

	/**
	 * @param WC_Order $order Objednávka.
	 * @return float
	 */
	private function direct_component_units( WC_Order $order ) {
		return $this->units_in_order( $order, 'component' );
	}

	/**
	 * @param WC_Order $order Objednávka.
	 * @param string   $role  component alebo assembled.
	 * @return float
	 */
	private function units_in_order( WC_Order $order, $role ) {
		$units = 0.0;
		foreach ( $order->get_items( 'line_item' ) as $item ) {
			if ( $this->item_matches( $item, $role ) ) {
				$units += (float) $item->get_quantity();
			}
		}

		return (float) wc_stock_amount( $units );
	}

	/**
	 * @param WC_Order_Item $item Položka.
	 * @param string        $role component alebo assembled.
	 * @return bool
	 */
	private function item_matches( $item, $role ) {
		if ( ! is_object( $item ) || ! is_callable( array( $item, 'get_product_id' ) ) ) {
			return false;
		}

		$configured   = 'component' === $role ? $this->component_id() : $this->assembled_id();
		$product_id   = (int) $item->get_product_id();
		$variation_id = is_callable( array( $item, 'get_variation_id' ) ) ? (int) $item->get_variation_id() : 0;
		$product      = is_callable( array( $item, 'get_product' ) ) ? $item->get_product() : null;

		return $this->ids_match( $product_id, $variation_id, $configured, $product instanceof WC_Product ? $product : null );
	}

	/**
	 * @param int             $product_id   ID rodiča alebo jednoduchého produktu.
	 * @param int             $variation_id ID variácie.
	 * @param int             $configured   Nastavené ID.
	 * @param WC_Product|null $product      Objekt produktu.
	 * @return bool
	 */
	private function ids_match( $product_id, $variation_id, $configured, $product = null ) {
		$configured = (int) $configured;
		if ( $configured <= 0 ) {
			return false;
		}

		if ( (int) $variation_id === $configured || (int) $product_id === $configured ) {
			return true;
		}

		return $product instanceof WC_Product && $this->product_matches( $product, $configured );
	}

	/**
	 * @param WC_Product $product      Produkt z výkladu.
	 * @param int        $configured   Nastavené ID.
	 * @return bool
	 */
	private function product_matches( WC_Product $product, $configured ) {
		$configured = (int) $configured;
		if ( (int) $product->get_id() === $configured ) {
			return true;
		}

		return (int) $product->get_parent_id() === $configured && $this->is_variable_product( $configured );
	}

	/**
	 * Variabilný rodič platí pre všetky svoje variácie. Konkrétna variácia platí len sama pre seba.
	 *
	 * @param int $product_id ID.
	 * @return bool
	 */
	private function is_variable_product( $product_id ) {
		$product_id = (int) $product_id;
		if ( ! isset( $this->variable_cache[ $product_id ] ) ) {
			$product = wc_get_product( $product_id );
			$this->variable_cache[ $product_id ] = $product && $product->is_type( 'variable' );
		}

		return $this->variable_cache[ $product_id ];
	}

	/**
	 * @param WC_Product $product Produkt.
	 * @return bool
	 */
	private function is_assembled_product( WC_Product $product ) {
		return $this->ready() && $this->product_matches( $product, $this->assembled_id() );
	}

	/**
	 * @param WC_Product $product Produkt, ktorému WooCommerce práve menil sklad.
	 * @return bool
	 */
	private function is_component_stock_owner( WC_Product $product ) {
		if ( ! $this->ready() ) {
			return false;
		}

		$component_id = $this->component_id();
		if ( (int) $product->get_id() === $component_id ) {
			return true;
		}

		$component = $this->component();
		return $component && (int) $product->get_id() === (int) $component->get_stock_managed_by_id();
	}

	/**
	 * @param int $product_id ID.
	 * @return bool
	 */
	private function is_component_id( $product_id ) {
		if ( ! $this->ready() ) {
			return false;
		}

		$product_id = (int) $product_id;
		if ( $product_id === $this->component_id() ) {
			return true;
		}

		$component = $this->component();
		return $component && $product_id === (int) $component->get_stock_managed_by_id();
	}

	/**
	 * @param float $assembled_qty Kusy zariadenia.
	 * @return float
	 */
	private function bom_units_for_quantity( $assembled_qty ) {
		return (float) wc_stock_amount( (float) $assembled_qty * $this->ratio() );
	}

	/**
	 * @return float
	 */
	private function ratio() {
		$ratio = apply_filters( self::RATIO_FILTER, 1 );
		$ratio = is_numeric( $ratio ) ? (float) $ratio : 1.0;

		return $ratio > 0 ? $ratio : 1.0;
	}

	/**
	 * Čítanie skladu komponentu z filtra produktu B. Stráži rekurziu.
	 *
	 * @return WC_Product|null
	 */
	private function read_component() {
		$this->mirroring = true;
		$component       = $this->component();
		$this->mirroring = false;

		return $component;
	}

	/**
	 * @param WC_Order $order Objednávka.
	 * @return float
	 */
	private function ledger( WC_Order $order ) {
		return max( 0.0, (float) $order->get_meta( self::LEDGER, true ) );
	}

	/**
	 * @param WC_Order $order  Objednávka.
	 * @param float    $amount Zostávajúci odpočet komponentu.
	 * @return void
	 */
	private function set_ledger( WC_Order $order, $amount ) {
		$amount = (float) wc_stock_amount( max( 0, $amount ) );
		$order->update_meta_data( self::LEDGER, $amount );
		$order->save_meta_data();
	}

	/**
	 * @param mixed $order Objednávka alebo jej ID.
	 * @return WC_Order|null
	 */
	private function as_order( $order ) {
		if ( $order instanceof WC_Order ) {
			return $order;
		}

		$loaded = wc_get_order( $order );
		return $loaded instanceof WC_Order ? $loaded : null;
	}

	/**
	 * @return array{component_id:int,assembled_id:int}
	 */
	private function settings() {
		if ( is_array( $this->settings_override ) ) {
			return array(
				'component_id' => isset( $this->settings_override['component_id'] ) ? (int) $this->settings_override['component_id'] : 0,
				'assembled_id' => isset( $this->settings_override['assembled_id'] ) ? (int) $this->settings_override['assembled_id'] : 0,
			);
		}

		$stored = get_option( self::OPTION, array() );
		if ( ! is_array( $stored ) ) {
			$stored = array();
		}

		return array(
			'component_id' => isset( $stored['component_id'] ) ? (int) $stored['component_id'] : 0,
			'assembled_id' => isset( $stored['assembled_id'] ) ? (int) $stored['assembled_id'] : 0,
		);
	}

	/**
	 * @return int
	 */
	private function component_id() {
		$settings = $this->settings();
		return (int) $settings['component_id'];
	}

	/**
	 * @return int
	 */
	private function assembled_id() {
		$settings = $this->settings();
		return (int) $settings['assembled_id'];
	}

	/**
	 * @return bool
	 */
	private function ready() {
		return $this->component_id() > 0 && $this->assembled_id() > 0 && $this->component_id() !== $this->assembled_id();
	}

	/**
	 * @return WC_Product|null
	 */
	private function component() {
		if ( ! $this->component_loaded ) {
			$this->component_loaded  = true;
			$product                 = $this->component_id() ? wc_get_product( $this->component_id() ) : null;
			$this->component_product = $product instanceof WC_Product ? $product : null;
		}

		return $this->component_product;
	}

	/**
	 * @return WC_Product|null
	 */
	private function assembled() {
		if ( ! $this->assembled_loaded ) {
			$this->assembled_loaded  = true;
			$product                 = $this->assembled_id() ? wc_get_product( $this->assembled_id() ) : null;
			$this->assembled_product = $product instanceof WC_Product ? $product : null;
		}

		return $this->assembled_product;
	}

	/**
	 * @return void
	 */
	private function flush_product_cache() {
		$this->component_loaded  = false;
		$this->component_product = null;
		$this->assembled_loaded  = false;
		$this->assembled_product = null;
		$this->variable_cache    = array();
	}

	/**
	 * @return WC_Cart|null
	 */
	private function cart() {
		if ( ! function_exists( 'WC' ) ) {
			return null;
		}

		$wc = WC();
		if ( ! $wc || ! $wc->cart ) {
			return null;
		}

		return $wc->cart;
	}

	/**
	 * @return int
	 */
	private function session_order_id() {
		if ( ! function_exists( 'WC' ) ) {
			return 0;
		}

		$wc = WC();
		if ( ! $wc || ! $wc->session ) {
			return 0;
		}

		$awaiting = absint( $wc->session->get( 'order_awaiting_payment' ) );
		if ( $awaiting ) {
			return $awaiting;
		}

		return absint( $wc->session->get( 'store_api_draft_order', 0 ) );
	}

	/**
	 * @param string $message Správa do denníka WooCommerce.
	 * @return void
	 */
	private function log( $message ) {
		if ( ! function_exists( 'wc_get_logger' ) ) {
			return;
		}

		wc_get_logger()->error( $message, array( 'source' => 'glukoview-sklad-bom' ) );
	}
}
