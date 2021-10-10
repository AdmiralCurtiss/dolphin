// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/RiivolutionBootWidget.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMetaType>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "DiscIO/RiivolutionParser.h"
#include "DiscIO/RiivolutionPatcher.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

struct GuiRiivolutionPatchIndex
{
  size_t m_disc_index;
  size_t m_section_index;
  size_t m_option_index;
  size_t m_choice_index;
};

Q_DECLARE_METATYPE(GuiRiivolutionPatchIndex);

RiivolutionBootWidget::RiivolutionBootWidget(std::string game_id, std::optional<u16> revision,
                                             std::optional<u8> disc, QWidget* parent)
    : QDialog(parent), m_game_id(std::move(game_id)),
      m_revision(revision.has_value() ? static_cast<int>(*revision) : -1),
      m_disc_number(disc.has_value() ? static_cast<int>(*disc) : -1)
{
  setWindowTitle(tr("Start with Riivolution Patches"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateWidgets();
  LoadMatchingXMLs();

  resize(QSize(400, 600));
}

RiivolutionBootWidget::~RiivolutionBootWidget() = default;

void RiivolutionBootWidget::CreateWidgets()
{
  auto* open_xml_button = new QPushButton(tr("Open Riivolution XML..."));
  auto* boot_game_button = new QPushButton(tr("Start"));
  boot_game_button->setDefault(true);
  auto* group_box = new QGroupBox();
  auto* scroll_area = new QScrollArea();

  auto* stretch_helper = new QVBoxLayout();
  m_patch_section_layout = new QVBoxLayout();
  stretch_helper->addLayout(m_patch_section_layout);
  stretch_helper->addStretch();
  group_box->setLayout(stretch_helper);
  scroll_area->setWidget(group_box);
  scroll_area->setWidgetResizable(true);

  auto* button_layout = new QHBoxLayout();
  button_layout->addStretch();
  button_layout->addWidget(open_xml_button, 0, Qt::AlignRight);
  button_layout->addWidget(boot_game_button, 0, Qt::AlignRight);

  auto* layout = new QVBoxLayout();
  layout->addWidget(scroll_area);
  layout->addLayout(button_layout);
  setLayout(layout);

  connect(open_xml_button, &QPushButton::clicked, this, &RiivolutionBootWidget::OpenXML);
  connect(boot_game_button, &QPushButton::clicked, this, &RiivolutionBootWidget::BootGame);
}

void RiivolutionBootWidget::LoadMatchingXMLs()
{
  const std::string& riivolution_dir = File::GetUserPath(D_RIIVOLUTION_IDX);
  for (const std::string& path : Common::DoFileSearch({riivolution_dir + "riivolution"}, {".xml"}))
  {
    auto parsed = DiscIO::Riivolution::ParseFile(path);
    if (!parsed || !parsed->IsValidForGame(m_game_id, m_revision, m_disc_number))
      continue;
    MakeGUIForParsedFile(path, *parsed);
  }
}

void RiivolutionBootWidget::OpenXML()
{
  const std::string& riivolution_dir = File::GetUserPath(D_RIIVOLUTION_IDX);
  QStringList paths = QFileDialog::getOpenFileNames(
      this, tr("Select Riivolution XML file"), QString::fromStdString(riivolution_dir),
      QStringLiteral("%1 (*.xml);;%2 (*)").arg(tr("Riivolution XML files")).arg(tr("All Files")));
  if (paths.isEmpty())
    return;

  for (const QString& path : paths)
  {
    std::string p = path.toStdString();
    auto parsed = DiscIO::Riivolution::ParseFile(p);
    if (!parsed)
    {
      ModalMessageBox::warning(
          this, tr("Failed loading XML."),
          tr("Did not recognize the given file as a valid Riivolution XML file."));
      continue;
    }

    if (!parsed->IsValidForGame(m_game_id, m_revision, m_disc_number))
    {
      ModalMessageBox::warning(
          this, tr("Invalid game."),
          tr("The loaded patches are not for the selected game or game revision."));
      continue;
    }

    MakeGUIForParsedFile(p, *parsed);
  }
}

static std::string FindRoot(std::string_view path)
{
  // Try to set the virtual SD root to directory one up from current.
  // This mimics where the XML would be on a real SD card.
  std::string contained_dir;
  if (SplitPath(path, &contained_dir, nullptr, nullptr))
  {
    std::string parent_dir;
    contained_dir.pop_back();
    if (SplitPath(contained_dir, &parent_dir, nullptr, nullptr))
      return parent_dir;
  }
  return File::GetUserPath(D_RIIVOLUTION_IDX);
}

void RiivolutionBootWidget::MakeGUIForParsedFile(const std::string& path,
                                                 DiscIO::Riivolution::Disc input_disc)
{
  const size_t disc_index = m_discs.size();
  const auto& disc = m_discs.emplace_back(DiscWithRoot{std::move(input_disc), FindRoot(path)});

  std::string filename;
  std::string extension;
  SplitPath(path, nullptr, &filename, &extension);
  auto* disc_box = new QGroupBox(QString::fromStdString(filename + extension));
  QVBoxLayout* disc_layout = new QVBoxLayout();
  disc_box->setLayout(disc_layout);

  auto* xml_root_line_edit = new QLineEdit(QString::fromStdString(disc.m_root));
  xml_root_line_edit->setReadOnly(true);
  auto* xml_root_layout = new QHBoxLayout();
  QPushButton* xml_root_open = new QPushButton(QStringLiteral("..."));
  xml_root_layout->addWidget(new QLabel(tr("Root:")), 0);
  xml_root_layout->addWidget(xml_root_line_edit, 0);
  xml_root_layout->addWidget(xml_root_open, 0);
  disc_layout->addLayout(xml_root_layout);
  connect(xml_root_open, &QPushButton::clicked, this, [this, xml_root_line_edit, disc_index]() {
    QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(
        this, tr("Select the Virtual SD Card Root"), xml_root_line_edit->text()));
    if (!dir.isEmpty())
    {
      xml_root_line_edit->setText(dir);
      m_discs[disc_index].m_root = dir.toStdString();
    }
  });

  for (size_t section_index = 0; section_index < disc.m_disc.m_sections.size(); ++section_index)
  {
    const auto& section = disc.m_disc.m_sections[section_index];
    auto* group_box = new QGroupBox(QString::fromStdString(section.m_name));
    auto* grid_layout = new QGridLayout();
    group_box->setLayout(grid_layout);

    int row = 0;
    for (size_t option_index = 0; option_index < section.m_options.size(); ++option_index)
    {
      const auto& option = section.m_options[option_index];
      auto* label = new QLabel(QString::fromStdString(option.m_name));
      auto* selection = new QComboBox();
      const GuiRiivolutionPatchIndex gui_disabled_index{disc_index, section_index, option_index, 0};
      selection->addItem(tr("Disabled"), QVariant::fromValue(gui_disabled_index));
      for (size_t choice_index = 0; choice_index < option.m_choices.size(); ++choice_index)
      {
        const auto& choice = option.m_choices[choice_index];
        const GuiRiivolutionPatchIndex gui_index{disc_index, section_index, option_index,
                                                 choice_index + 1};
        selection->addItem(QString::fromStdString(choice.m_name), QVariant::fromValue(gui_index));
      }
      if (option.m_selected_choice <= option.m_choices.size())
        selection->setCurrentIndex(static_cast<int>(option.m_selected_choice));

      connect(selection, qOverload<int>(&QComboBox::currentIndexChanged), this,
              [this, selection](int idx) {
                const auto gui_index = selection->currentData().value<GuiRiivolutionPatchIndex>();
                auto& disc = m_discs[gui_index.m_disc_index].m_disc;
                auto& section = disc.m_sections[gui_index.m_section_index];
                auto& option = section.m_options[gui_index.m_option_index];
                option.m_selected_choice = static_cast<u32>(gui_index.m_choice_index);
              });

      grid_layout->addWidget(label, row, 0, 1, 1);
      grid_layout->addWidget(selection, row, 1, 1, 1);
      ++row;
    }

    disc_layout->addWidget(group_box);
  }

  m_patch_section_layout->addWidget(disc_box);
}

void RiivolutionBootWidget::BootGame()
{
  m_patches.clear();
  for (const auto& disc : m_discs)
  {
    auto patches = disc.m_disc.GeneratePatches(m_game_id);

    // set the file loader for each patch
    for (auto& patch : patches)
    {
      patch.m_file_data_loader = std::make_shared<DiscIO::Riivolution::FileDataLoaderHostFS>(
          disc.m_root, disc.m_disc.m_xml_path, patch.m_root);
    }

    m_patches.insert(m_patches.end(), patches.begin(), patches.end());
  }

  m_should_boot = true;
  close();
}